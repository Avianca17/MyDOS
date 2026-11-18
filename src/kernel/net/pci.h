#ifndef MYDOS_PCI_H
#define MYDOS_PCI_H

#include <stdint.h>

#define PCI_CLASS_NETWORK 0x02
#define PCI_SUBCLASS_ETHERNET 0x00

typedef struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
} pci_device_t;

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function,
                           uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t device, uint8_t function,
                        uint8_t offset, uint32_t value);
int pci_find_device(uint16_t vendor_id, uint16_t device_id,
                    unsigned int index, pci_device_t *result);
int pci_find_class(uint8_t class_code, uint8_t subclass,
                   unsigned int index, pci_device_t *result);
int pci_enable_device(const pci_device_t *device, int io, int memory,
                      int bus_master);
uint32_t pci_bar_address(const pci_device_t *device, unsigned int bar,
                         int *is_io);

#endif
