#include <net/pci.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function,
                           uint8_t offset) {
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) |
                       (offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t device, uint8_t function,
                        uint8_t offset, uint32_t value) {
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) |
                       (offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

static void pci_fill_device(uint8_t bus, uint8_t device, uint8_t function,
                            pci_device_t *result) {
    uint32_t vendor = pci_config_read32(bus, device, function, 0x00);
    uint32_t class_info = pci_config_read32(bus, device, function, 0x08);
    uint32_t header = pci_config_read32(bus, device, function, 0x0C);

    result->bus = bus;
    result->device = device;
    result->function = function;
    result->vendor_id = (uint16_t)(vendor & 0xFFFFu);
    result->device_id = (uint16_t)(vendor >> 16);
    result->class_code = (uint8_t)(class_info >> 24);
    result->subclass = (uint8_t)(class_info >> 16);
    result->prog_if = (uint8_t)(class_info >> 8);
    result->header_type = (uint8_t)(header >> 16);
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id,
                    unsigned int index, pci_device_t *result) {
    unsigned int seen = 0;

    for (unsigned int bus = 0; bus < 256; bus++) {
        for (unsigned int device = 0; device < 32; device++) {
            uint32_t vendor = pci_config_read32((uint8_t)bus, (uint8_t)device, 0, 0x00);
            if ((vendor & 0xFFFFu) == 0xFFFFu) {
                continue;
            }

            uint32_t header = pci_config_read32((uint8_t)bus, (uint8_t)device, 0, 0x0C);
            unsigned int functions = (header & 0x00800000u) ? 8u : 1u;

            for (unsigned int function = 0; function < functions; function++) {
                uint32_t id = pci_config_read32((uint8_t)bus, (uint8_t)device,
                                                (uint8_t)function, 0x00);
                if ((uint16_t)(id & 0xFFFFu) != vendor_id ||
                    (uint16_t)(id >> 16) != device_id) {
                    continue;
                }

                if (seen == index) {
                    pci_fill_device((uint8_t)bus, (uint8_t)device,
                                    (uint8_t)function, result);
                    return 1;
                }
                seen++;
            }
        }
    }

    return 0;
}

int pci_find_class(uint8_t class_code, uint8_t subclass,
                   unsigned int index, pci_device_t *result) {
    unsigned int seen = 0;

    for (unsigned int bus = 0; bus < 256; bus++) {
        for (unsigned int device = 0; device < 32; device++) {
            uint32_t vendor = pci_config_read32((uint8_t)bus, (uint8_t)device, 0, 0x00);
            if ((vendor & 0xFFFFu) == 0xFFFFu) {
                continue;
            }

            uint32_t header = pci_config_read32((uint8_t)bus, (uint8_t)device, 0, 0x0C);
            unsigned int functions = (header & 0x00800000u) ? 8u : 1u;

            for (unsigned int function = 0; function < functions; function++) {
                uint32_t class_info = pci_config_read32((uint8_t)bus, (uint8_t)device,
                                                        (uint8_t)function, 0x08);
                if ((uint8_t)(class_info >> 24) != class_code ||
                    (uint8_t)(class_info >> 16) != subclass) {
                    continue;
                }

                if (seen == index) {
                    pci_fill_device((uint8_t)bus, (uint8_t)device,
                                    (uint8_t)function, result);
                    return 1;
                }
                seen++;
            }
        }
    }

    return 0;
}

int pci_enable_device(const pci_device_t *device, int io, int memory,
                      int bus_master) {
    uint32_t command = pci_config_read32(device->bus, device->device,
                                         device->function, 0x04);
    uint32_t flags = command & 0xFFFFu;

    if (io) {
        flags |= 0x0001u;
    }
    if (memory) {
        flags |= 0x0002u;
    }
    if (bus_master) {
        flags |= 0x0004u;
    }

    if (flags == (command & 0xFFFFu)) {
        return 1;
    }

    pci_config_write32(device->bus, device->device, device->function, 0x04,
                       (command & 0xFFFF0000u) | flags);
    command = pci_config_read32(device->bus, device->device, device->function, 0x04);

    return ((command & 0xFFFFu) & flags) == flags;
}

uint32_t pci_bar_address(const pci_device_t *device, unsigned int bar,
                         int *is_io) {
    uint8_t offset = (uint8_t)(0x10u + bar * 4u);
    uint32_t value = pci_config_read32(device->bus, device->device,
                                       device->function, offset);

    if (is_io != 0) {
        *is_io = (value & 0x01u) != 0;
    }

    if (value == 0 || value == 0xFFFFFFFFu) {
        return 0;
    }

    if (value & 0x01u) {
        return value & 0xFFFFFFFCu;
    }

    uint8_t type = (uint8_t)((value >> 1) & 0x03u);
    if (type == 0x02u) {
        uint32_t high = pci_config_read32(device->bus, device->device,
                                          device->function, (uint8_t)(offset + 4u));
        if (high != 0) {
            return 0;
        }
    }

    return value & 0xFFFFFFF0u;
}
