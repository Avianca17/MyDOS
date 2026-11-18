#include <mydos.h>
#include <net/net.h>
#include <net/pci.h>

#define E1000_VENDOR 0x8086

#define E1000_CTRL 0x00000
#define E1000_STATUS 0x00008
#define E1000_EERD 0x00014
#define E1000_ICR 0x000C0
#define E1000_IMC 0x000D8
#define E1000_RCTL 0x00100
#define E1000_TCTL 0x00400
#define E1000_TIPG 0x00410
#define E1000_RDBAL 0x02800
#define E1000_RDBAH 0x02804
#define E1000_RDLEN 0x02808
#define E1000_RDH 0x02810
#define E1000_RDT 0x02818
#define E1000_TDBAL 0x03800
#define E1000_TDBAH 0x03804
#define E1000_TDLEN 0x03808
#define E1000_TDH 0x03810
#define E1000_TDT 0x03818
#define E1000_RAL0 0x05400
#define E1000_RAH0 0x05404

#define E1000_CTRL_SLU 0x00000040u
#define E1000_CTRL_RST 0x04000000u

#define E1000_STATUS_LU 0x00000002u

#define E1000_EERD_START 0x00000001u
#define E1000_EERD_DONE 0x00000010u

#define E1000_RCTL_EN 0x00000002u
#define E1000_RCTL_SBP 0x00000004u
#define E1000_RCTL_UPE 0x00000008u
#define E1000_RCTL_MPE 0x00000010u
#define E1000_RCTL_LPE 0x00000020u
#define E1000_RCTL_BAM 0x00008000u
#define E1000_RCTL_SECRC 0x04000000u

#define E1000_TCTL_EN 0x00000002u
#define E1000_TCTL_PSP 0x00000008u
#define E1000_TCTL_CT 0x00000100u
#define E1000_TCTL_COLD 0x00040000u

#define E1000_TXD_CMD_EOP 0x01u
#define E1000_TXD_CMD_IFCS 0x02u
#define E1000_TXD_CMD_RS 0x08u
#define E1000_TXD_STAT_DD 0x01u

#define E1000_RXD_STAT_DD 0x01u
#define E1000_RXD_STAT_EOP 0x02u

#define E1000_RX_COUNT 32
#define E1000_TX_COUNT 8
#define E1000_RX_BUFFER_SIZE 2048
#define E1000_TX_BUFFER_SIZE 2048

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint16_t length;
    uint8_t checksum_offset;
    uint8_t command;
    uint8_t status;
    uint8_t checksum_start;
    uint16_t special;
} e1000_tx_descriptor_t;

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} e1000_rx_descriptor_t;

static volatile uint8_t *e1000_mmio;
static e1000_tx_descriptor_t tx_descriptors[E1000_TX_COUNT] __attribute__((aligned(16)));
static e1000_rx_descriptor_t rx_descriptors[E1000_RX_COUNT] __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_TX_COUNT][E1000_TX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t rx_buffers[E1000_RX_COUNT][E1000_RX_BUFFER_SIZE] __attribute__((aligned(16)));
static unsigned int tx_current;
static unsigned int rx_current;

static inline void io_delay(void) {
    __asm__ volatile("outb %%al, $0x80" : : "a"(0));
}

static inline void memory_barrier(void) {
    __asm__ volatile("" : : : "memory");
}

static uint32_t e1000_read(uint32_t reg) {
    return *(volatile uint32_t *)(e1000_mmio + reg);
}

static void e1000_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *)(e1000_mmio + reg) = value;
}

static int e1000_supported(uint16_t device_id) {
    static const uint16_t ids[] = {
        0x1000, 0x1001, 0x1004, 0x1008, 0x1009, 0x100C, 0x100D,
        0x100E, 0x100F, 0x1010, 0x1011, 0x1012, 0x1013, 0x1014,
        0x1015, 0x1016, 0x1017, 0x1018, 0x1019, 0x101A, 0x101D,
        0x101E, 0x1026, 0x1027, 0x1028, 0x1075, 0x1076, 0x1077,
        0x1078, 0x1079, 0x107A, 0x107B, 0x107C, 0x108A, 0x1099,
        0x10B5, 0x2E6E,
    };

    for (unsigned int i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        if (ids[i] == device_id) {
            return 1;
        }
    }

    return 0;
}

static int e1000_find(pci_device_t *pci) {
    for (unsigned int index = 0; index < 32; index++) {
        pci_device_t candidate;
        if (!pci_find_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET, index, &candidate)) {
            break;
        }
        if (candidate.vendor_id == E1000_VENDOR && e1000_supported(candidate.device_id)) {
            *pci = candidate;
            return 1;
        }
    }

    return 0;
}

static uint16_t e1000_read_eeprom(uint8_t address) {
    e1000_write(E1000_EERD, E1000_EERD_START | ((uint32_t)address << 8));

    for (unsigned int i = 0; i < 100000; i++) {
        uint32_t value = e1000_read(E1000_EERD);
        if (value & E1000_EERD_DONE) {
            return (uint16_t)(value >> 16);
        }
        io_delay();
    }

    return 0xFFFFu;
}

static void e1000_read_mac(uint8_t *mac, const pci_device_t *pci) {
    uint16_t words[3];

    for (unsigned int i = 0; i < 3; i++) {
        words[i] = e1000_read_eeprom((uint8_t)i);
    }

    int invalid = 1;
    for (unsigned int i = 0; i < 3; i++) {
        if (words[i] != 0x0000 && words[i] != 0xFFFF) {
            invalid = 0;
        }
    }

    if (invalid) {
        uint32_t low = e1000_read(E1000_RAL0);
        uint32_t high = e1000_read(E1000_RAH0);
        words[0] = (uint16_t)(low & 0xFFFFu);
        words[1] = (uint16_t)(low >> 16);
        words[2] = (uint16_t)(high & 0xFFFFu);
    }

    mac[0] = (uint8_t)(words[0] & 0xFFu);
    mac[1] = (uint8_t)(words[0] >> 8);
    mac[2] = (uint8_t)(words[1] & 0xFFu);
    mac[3] = (uint8_t)(words[1] >> 8);
    mac[4] = (uint8_t)(words[2] & 0xFFu);
    mac[5] = (uint8_t)(words[2] >> 8);

    int valid = 0;
    for (unsigned int i = 0; i < 6; i++) {
        if (mac[i] != 0x00 && mac[i] != 0xFF) {
            valid = 1;
        }
    }

    if (!valid) {
        mac[0] = 0x02;
        mac[1] = 0x4D;
        mac[2] = 0x59;
        mac[3] = pci->bus;
        mac[4] = pci->device;
        mac[5] = pci->function;
    }
}

static int e1000_send(const uint8_t *frame, uint16_t length) {
    if (e1000_mmio == 0 || frame == 0 || length == 0 ||
        length > E1000_TX_BUFFER_SIZE) {
        return 0;
    }

    unsigned int index = tx_current;
    volatile e1000_tx_descriptor_t *descriptor = &tx_descriptors[index];

    for (unsigned int i = 0; i < 1000000; i++) {
        if (descriptor->status & E1000_TXD_STAT_DD) {
            break;
        }
        io_delay();
    }

    if (!(descriptor->status & E1000_TXD_STAT_DD)) {
        return 0;
    }

    memcpy(tx_buffers[index], frame, length);
    descriptor->address = (uint64_t)(uintptr_t)tx_buffers[index];
    descriptor->length = length;
    descriptor->checksum_offset = 0;
    descriptor->checksum_start = 0;
    descriptor->special = 0;
    descriptor->status = 0;
    descriptor->command = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    memory_barrier();
    e1000_write(E1000_TDT, (index + 1) % E1000_TX_COUNT);
    tx_current = (index + 1) % E1000_TX_COUNT;

    for (unsigned int i = 0; i < 1000000; i++) {
        if (descriptor->status & E1000_TXD_STAT_DD) {
            return 1;
        }
        io_delay();
    }

    return 0;
}

static int e1000_receive(uint8_t *frame, uint16_t capacity) {
    if (e1000_mmio == 0 || frame == 0 || capacity == 0) {
        return 0;
    }

    for (unsigned int attempt = 0; attempt < E1000_RX_COUNT; attempt++) {
        unsigned int index = rx_current;
        volatile e1000_rx_descriptor_t *descriptor = &rx_descriptors[index];

        if (!(descriptor->status & E1000_RXD_STAT_DD)) {
            return 0;
        }

        memory_barrier();
        uint16_t length = descriptor->length;
        uint8_t errors = descriptor->errors;
        uint8_t status = descriptor->status;

        descriptor->status = 0;
        e1000_write(E1000_RDT, index);
        rx_current = (index + 1) % E1000_RX_COUNT;

        if (errors == 0 && (status & E1000_RXD_STAT_EOP) &&
            length > 0 && length <= capacity) {
            memcpy(frame, rx_buffers[index], length);
            return length;
        }
    }

    return 0;
}

int e1000_init(net_device_t *device) {
    pci_device_t pci;
    if (!e1000_find(&pci)) {
        return 0;
    }

    if (!pci_enable_device(&pci, 0, 1, 1)) {
        return 0;
    }

    int is_io = 0;
    uint32_t bar = pci_bar_address(&pci, 0, &is_io);
    if (bar == 0 || is_io) {
        return 0;
    }

    e1000_mmio = (volatile uint8_t *)(uintptr_t)bar;

    e1000_write(E1000_IMC, 0xFFFFFFFFu);
    e1000_read(E1000_ICR);

    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);
    for (unsigned int i = 0; i < 1000000; i++) {
        if (!(e1000_read(E1000_CTRL) & E1000_CTRL_RST)) {
            break;
        }
        io_delay();
    }
    if (e1000_read(E1000_CTRL) & E1000_CTRL_RST) {
        e1000_mmio = 0;
        return 0;
    }

    e1000_write(E1000_IMC, 0xFFFFFFFFu);
    e1000_read(E1000_ICR);
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_SLU);

    e1000_read_mac(device->mac, &pci);
    e1000_write(E1000_RAL0,
                (uint32_t)device->mac[0] |
                ((uint32_t)device->mac[1] << 8) |
                ((uint32_t)device->mac[2] << 16) |
                ((uint32_t)device->mac[3] << 24));
    e1000_write(E1000_RAH0,
                0x80000000u |
                (uint32_t)device->mac[4] |
                ((uint32_t)device->mac[5] << 8));

    memset(tx_descriptors, 0, sizeof(tx_descriptors));
    for (unsigned int i = 0; i < E1000_TX_COUNT; i++) {
        tx_descriptors[i].status = E1000_TXD_STAT_DD;
    }
    memset(rx_descriptors, 0, sizeof(rx_descriptors));
    for (unsigned int i = 0; i < E1000_RX_COUNT; i++) {
        rx_descriptors[i].address = (uint64_t)(uintptr_t)rx_buffers[i];
    }

    tx_current = 0;
    rx_current = 0;

    e1000_write(E1000_TDBAL, (uint32_t)(uintptr_t)tx_descriptors);
    e1000_write(E1000_TDBAH, 0);
    e1000_write(E1000_TDLEN, sizeof(tx_descriptors));
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);

    e1000_write(E1000_RDBAL, (uint32_t)(uintptr_t)rx_descriptors);
    e1000_write(E1000_RDBAH, 0);
    e1000_write(E1000_RDLEN, sizeof(rx_descriptors));
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_RX_COUNT - 1);

    e1000_write(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_SBP |
                            E1000_RCTL_UPE | E1000_RCTL_MPE |
                            E1000_RCTL_LPE | E1000_RCTL_BAM |
                            E1000_RCTL_SECRC);

    for (unsigned int i = 0; i < 2000000; i++) {
        if (e1000_read(E1000_STATUS) & E1000_STATUS_LU) {
            break;
        }
        io_delay();
    }

    e1000_write(E1000_TIPG, 0x0060200A);
    e1000_write(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP |
                            E1000_TCTL_CT | E1000_TCTL_COLD);

    device->name = "Intel e1000";
    device->send = e1000_send;
    device->receive = e1000_receive;
    return 1;
}
