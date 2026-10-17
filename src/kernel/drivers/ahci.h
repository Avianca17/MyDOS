#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>

int ahci_init(void);
unsigned int ahci_disk_count(void);
uint32_t ahci_sector_count(unsigned int disk);
int ahci_read(unsigned int disk, uint32_t sector, void *buffer);
int ahci_write(unsigned int disk, uint32_t sector, const void *buffer);
int ahci_flush(unsigned int disk);

#endif
