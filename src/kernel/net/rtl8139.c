#include <mydos.h>
#include <net/net.h>
#include <net/pci.h>

#define RTL8139_VENDOR 0x10EC
#define RTL8139_DEVICE 0x8139
#define RTL8129_DEVICE 0x8129

#define RTL_IDR0 0x00
#define RTL_TSD0 0x10
#define RTL_TSAD0 0x20
#define RTL_RBSTART 0x30
#define RTL_CR 0x37
#define RTL_CAPR 0x38
#define RTL_IMR 0x3C
#define RTL_ISR 0x3E
#define RTL_TCR 0x40
#define RTL_RCR 0x44
#define RTL_RXMISS 0x4C
#define RTL_9346CR 0x50
#define RTL_CONFIG1 0x52
#define RTL_HLTCLK 0x5B

#define RTL_CR_BUFE 0x01
#define RTL_CR_TE 0x04
#define RTL_CR_RE 0x08
#define RTL_CR_RST 0x10

#define RTL_9346_UNLOCK 0xC0
#define RTL_9346_LOCK 0x00

#define RTL_TSD_UNDERRUN 0x00004000u
#define RTL_TSD_STAT_OK 0x00008000u
#define RTL_TSD_ABORTED 0x80000000u
#define RTL_TSD_FLAGS (RTL_TSD_UNDERRUN | RTL_TSD_STAT_OK | RTL_TSD_ABORTED)

#define RTL_TCR_VALUE 0x03000680u
#define RTL_RCR_VALUE 0x0000E70Au

#define RTL_RX_RING_SIZE 8192
#define RTL_RX_BUFFER_SIZE 1792
#define RTL_TX_BUFFER_SIZE 1792
#define RTL_TX_COUNT 4

static uint16_t rtl_io_base;
static volatile uint8_t *rtl_mmio_base;
static uint8_t rx_ring[RTL_RX_RING_SIZE + 16] __attribute__((aligned(4)));
static uint8_t tx_buffers[RTL_TX_COUNT][RTL_TX_BUFFER_SIZE] __attribute__((aligned(4)));
static uint16_t rx_offset;
static unsigned int tx_current;
static uint8_t tx_used[RTL_TX_COUNT];

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_delay(void) {
    outb(0x80, 0);
}

static inline void memory_barrier(void) {
    __asm__ volatile("" : : : "memory");
}

static uint8_t rtl_read8(uint16_t reg) {
    if (rtl_mmio_base != 0) {
        return *(volatile uint8_t *)(rtl_mmio_base + reg);
    }
    return inb((uint16_t)(rtl_io_base + reg));
}

static uint16_t rtl_read16(uint16_t reg) {
    if (rtl_mmio_base != 0) {
        return *(volatile uint16_t *)(rtl_mmio_base + reg);
    }
    return inw((uint16_t)(rtl_io_base + reg));
}

static uint32_t rtl_read32(uint16_t reg) {
    if (rtl_mmio_base != 0) {
        return *(volatile uint32_t *)(rtl_mmio_base + reg);
    }
    return inl((uint16_t)(rtl_io_base + reg));
}

static void rtl_write8(uint16_t reg, uint8_t value) {
    if (rtl_mmio_base != 0) {
        *(volatile uint8_t *)(rtl_mmio_base + reg) = value;
        return;
    }
    outb((uint16_t)(rtl_io_base + reg), value);
}

static void rtl_write16(uint16_t reg, uint16_t value) {
    if (rtl_mmio_base != 0) {
        *(volatile uint16_t *)(rtl_mmio_base + reg) = value;
        return;
    }
    outw((uint16_t)(rtl_io_base + reg), value);
}

static void rtl_write32(uint16_t reg, uint32_t value) {
    if (rtl_mmio_base != 0) {
        *(volatile uint32_t *)(rtl_mmio_base + reg) = value;
        return;
    }
    outl((uint16_t)(rtl_io_base + reg), value);
}

static int rtl8139_find(pci_device_t *pci) {
    for (unsigned int index = 0; index < 32; index++) {
        pci_device_t candidate;
        if (!pci_find_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET, index, &candidate)) {
            break;
        }
        if (candidate.vendor_id == RTL8139_VENDOR &&
            (candidate.device_id == RTL8139_DEVICE ||
             candidate.device_id == RTL8129_DEVICE)) {
            *pci = candidate;
            return 1;
        }
    }

    return 0;
}

static int rtl8139_reset(void) {
    rtl_write8(RTL_CR, RTL_CR_RST);

    for (unsigned int i = 0; i < 1000000; i++) {
        if (!(rtl_read8(RTL_CR) & RTL_CR_RST)) {
            return 1;
        }
        io_delay();
    }

    return 0;
}

static void rtl8139_advance_rx(uint16_t size) {
    uint32_t next = (uint32_t)rx_offset + size + 4;
    next = (next + 3u) & ~3u;
    rx_offset = (uint16_t)(next % RTL_RX_RING_SIZE);
    rtl_write16(RTL_CAPR, (uint16_t)(rx_offset - 16));
}

static int rtl8139_send(const uint8_t *frame, uint16_t length) {
    if ((rtl_io_base == 0 && rtl_mmio_base == 0) || frame == 0 ||
        length == 0 || length > RTL_TX_BUFFER_SIZE) {
        return 0;
    }

    unsigned int index = tx_current;

    if (tx_used[index]) {
        uint32_t status = rtl_read32((uint16_t)(RTL_TSD0 + index * 4));
        for (unsigned int i = 0; i < 1000000; i++) {
            if (status & RTL_TSD_FLAGS) {
                break;
            }
            io_delay();
            status = rtl_read32((uint16_t)(RTL_TSD0 + index * 4));
        }
        if (!(status & RTL_TSD_FLAGS)) {
            return 0;
        }
    }

    memcpy(tx_buffers[index], frame, length);
    if (length < 60) {
        memset(tx_buffers[index] + length, 0, 60 - length);
        length = 60;
    }

    rtl_write32((uint16_t)(RTL_TSAD0 + index * 4),
                (uint32_t)(uintptr_t)tx_buffers[index]);
    memory_barrier();
    rtl_write32((uint16_t)(RTL_TSD0 + index * 4), 0x00080000u | length);
    tx_used[index] = 1;

    uint32_t status = 0;
    for (unsigned int i = 0; i < 1000000; i++) {
        status = rtl_read32((uint16_t)(RTL_TSD0 + index * 4));
        if (status & RTL_TSD_FLAGS) {
            break;
        }
        io_delay();
    }

    tx_current = (index + 1) % RTL_TX_COUNT;
    return (status & RTL_TSD_STAT_OK) != 0;
}

static int rtl8139_receive(uint8_t *frame, uint16_t capacity) {
    if ((rtl_io_base == 0 && rtl_mmio_base == 0) || frame == 0 || capacity == 0) {
        return 0;
    }

    for (unsigned int attempt = 0; attempt < 16; attempt++) {
        if (rtl_read8(RTL_CR) & RTL_CR_BUFE) {
            return 0;
        }

        uint32_t header = *(volatile uint32_t *)(rx_ring + rx_offset);
        uint16_t status = (uint16_t)(header & 0xFFFFu);
        uint16_t size = (uint16_t)(header >> 16);

        if (size < 8 || size > RTL_RX_RING_SIZE) {
            rx_offset = 0;
            rtl_write16(RTL_CAPR, 0xFFF0u);
            return 0;
        }

        uint16_t packet_length = (uint16_t)(size - 4);
        if ((status & 0x0001u) && packet_length >= 14 &&
            packet_length <= capacity) {
            uint16_t start = (uint16_t)(rx_offset + 4);
            if (start >= RTL_RX_RING_SIZE) {
                start = (uint16_t)(start - RTL_RX_RING_SIZE);
            }

            uint16_t first = (uint16_t)(RTL_RX_RING_SIZE - start);
            if (first > packet_length) {
                first = packet_length;
            }

            memcpy(frame, rx_ring + start, first);
            if (packet_length > first) {
                memcpy(frame + first, rx_ring, packet_length - first);
            }

            rtl8139_advance_rx(size);
            return packet_length;
        }

        rtl8139_advance_rx(size);
    }

    return 0;
}

int rtl8139_init(net_device_t *device) {
    pci_device_t pci;
    if (!rtl8139_find(&pci)) {
        return 0;
    }

    if (!pci_enable_device(&pci, 1, 1, 1)) {
        return 0;
    }

    int is_io = 0;
    uint32_t bar0 = pci_bar_address(&pci, 0, &is_io);
    if (bar0 != 0 && is_io) {
        rtl_io_base = (uint16_t)bar0;
        rtl_mmio_base = 0;
    } else {
        int bar_is_io = 0;
        uint32_t bar1 = pci_bar_address(&pci, 1, &bar_is_io);
        if (bar1 == 0 || bar_is_io) {
            return 0;
        }
        rtl_io_base = 0;
        rtl_mmio_base = (volatile uint8_t *)(uintptr_t)bar1;
    }

    rtl_write8(RTL_HLTCLK, 'R');

    if (!rtl8139_reset()) {
        rtl_io_base = 0;
        rtl_mmio_base = 0;
        return 0;
    }

    rtl_write8(RTL_9346CR, RTL_9346_UNLOCK);

    uint8_t config1 = rtl_read8(RTL_CONFIG1);
    rtl_write8(RTL_CONFIG1, (uint8_t)(config1 & ~0x03u));

    uint32_t id = rtl_read32(RTL_IDR0);
    uint16_t id_high = rtl_read16((uint16_t)(RTL_IDR0 + 4));
    device->mac[0] = (uint8_t)(id & 0xFFu);
    device->mac[1] = (uint8_t)((id >> 8) & 0xFFu);
    device->mac[2] = (uint8_t)((id >> 16) & 0xFFu);
    device->mac[3] = (uint8_t)((id >> 24) & 0xFFu);
    device->mac[4] = (uint8_t)(id_high & 0xFFu);
    device->mac[5] = (uint8_t)(id_high >> 8);

    int valid = 0;
    for (unsigned int i = 0; i < 6; i++) {
        if (device->mac[i] != 0x00 && device->mac[i] != 0xFF) {
            valid = 1;
        }
    }

    if (!valid) {
        device->mac[0] = 0x02;
        device->mac[1] = 0x52;
        device->mac[2] = 0x54;
        device->mac[3] = pci.bus;
        device->mac[4] = pci.device;
        device->mac[5] = pci.function;
    }

    for (unsigned int i = 0; i < 6; i++) {
        rtl_write8((uint16_t)(RTL_IDR0 + i), device->mac[i]);
    }

    memset(rx_ring, 0, sizeof(rx_ring));
    memset(tx_buffers, 0, sizeof(tx_buffers));
    memset(tx_used, 0, sizeof(tx_used));
    rx_offset = 0;
    tx_current = 0;

    rtl_write32(RTL_RBSTART, (uint32_t)(uintptr_t)rx_ring);
    rtl_write32(RTL_RCR, RTL_RCR_VALUE);
    rtl_write32(RTL_TCR, RTL_TCR_VALUE);

    for (unsigned int i = 0; i < RTL_TX_COUNT; i++) {
        rtl_write32((uint16_t)(RTL_TSAD0 + i * 4),
                    (uint32_t)(uintptr_t)tx_buffers[i]);
    }

    rtl_write8(RTL_9346CR, RTL_9346_LOCK);
    rtl_write16(RTL_IMR, 0);
    rtl_write16(RTL_ISR, 0xFFFFu);
    rtl_write32(RTL_RXMISS, 0);
    rtl_write8(RTL_CR, RTL_CR_RE | RTL_CR_TE);

    device->name = "Realtek RTL8139";
    device->send = rtl8139_send;
    device->receive = rtl8139_receive;
    return 1;
}
