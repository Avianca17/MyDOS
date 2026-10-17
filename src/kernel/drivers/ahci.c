#include "ahci.h"

#include <stddef.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_COMMAND 0x04
#define PCI_BAR5 0x24
#define PCI_CLASS_REVISION 0x08

#define AHCI_MAX_PORTS 32
#define AHCI_MAX_DISKS 4
#define AHCI_PCI_CLASS 0x010601u

#define HBA_GHC 0x04
#define HBA_PI 0x0C
#define HBA_PORT_BASE 0x100
#define HBA_PORT_STRIDE 0x80

#define PORT_CLB 0x00
#define PORT_FB 0x08
#define PORT_IS 0x10
#define PORT_CMD 0x18
#define PORT_TFD 0x20
#define PORT_SIG 0x24
#define PORT_SSTS 0x28
#define PORT_SERR 0x30
#define PORT_CI 0x38

#define PORT_CMD_ST (1u << 0)
#define PORT_CMD_FRE (1u << 4)
#define PORT_CMD_FR (1u << 14)
#define PORT_CMD_CR (1u << 15)
#define PORT_IS_TFES (1u << 30)
#define PORT_TFD_ERR (1u << 0)
#define PORT_TFD_BSY (1u << 7)
#define PORT_TFD_DRQ (1u << 3)
#define PORT_SIG_ATA 0x00000101u
#define PORT_SSTS_DET_MASK 0x0Fu
#define PORT_SSTS_DET_PRESENT 0x03u
#define PORT_SSTS_IPM_MASK 0x0F00u
#define PORT_SSTS_IPM_ACTIVE 0x0100u

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_READ_DMA 0xC8
#define ATA_CMD_WRITE_DMA 0xCA
#define ATA_CMD_FLUSH_CACHE 0xE7
#define ATA_LBA28_SECTORS 0x10000000u
#define AHCI_TIMEOUT 10000000u

typedef struct __attribute__((packed)) {
	uint32_t options;
	uint32_t transferred;
	uint32_t table_low;
	uint32_t table_high;
	uint32_t reserved[4];
} ahci_command_header_t;

typedef struct __attribute__((packed)) {
	uint8_t fis[64];
	uint8_t atapi_command[16];
	uint8_t reserved[48];
	struct __attribute__((packed)) {
		uint32_t address_low;
		uint32_t address_high;
		uint32_t reserved;
		uint32_t byte_count;
	} prdt[1];
	uint8_t trailing_reserved[112];
} ahci_command_table_t;

typedef struct __attribute__((aligned(1024))) {
	ahci_command_header_t headers[32];
} ahci_command_list_t;

typedef struct __attribute__((aligned(256))) {
	uint8_t bytes[256];
} ahci_received_fis_t;

static volatile uint32_t *hba;
static uint8_t disk_ports[AHCI_MAX_DISKS];
static uint32_t disk_sectors[AHCI_MAX_DISKS];
static uint8_t disk_flush_supported[AHCI_MAX_DISKS];
static unsigned int disk_total;

static ahci_command_list_t command_lists[AHCI_MAX_PORTS];
static ahci_received_fis_t received_fis[AHCI_MAX_PORTS];
static ahci_command_table_t command_tables[AHCI_MAX_PORTS]
	__attribute__((aligned(128)));
static uint8_t transfer_buffer[512] __attribute__((aligned(2)));

static void io_wait(void) {
	__asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

static void outl(uint16_t port, uint32_t value) {
	__asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static uint32_t inl(uint16_t port) {
	uint32_t value;
	__asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

static uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function,
                         uint8_t offset) {
	uint32_t address = 0x80000000u | ((uint32_t)bus << 16) |
		((uint32_t)device << 11) | ((uint32_t)function << 8) |
		(offset & 0xFCu);

	outl(PCI_CONFIG_ADDRESS, address);
	return inl(PCI_CONFIG_DATA);
}

static void pci_write(uint8_t bus, uint8_t device, uint8_t function,
                      uint8_t offset, uint32_t value) {
	uint32_t address = 0x80000000u | ((uint32_t)bus << 16) |
		((uint32_t)device << 11) | ((uint32_t)function << 8) |
		(offset & 0xFCu);

	outl(PCI_CONFIG_ADDRESS, address);
	outl(PCI_CONFIG_DATA, value);
}

static volatile uint32_t *find_ahci_controller(void) {
	for (unsigned int bus = 0; bus < 256; bus++) {
		for (unsigned int dev = 0; dev < 32; dev++) {
			uint32_t id = pci_read((uint8_t)bus, (uint8_t)dev, 0, 0);
			if ((id & 0xFFFFu) == 0xFFFFu) {
				continue;
			}

			uint32_t header = pci_read((uint8_t)bus, (uint8_t)dev, 0, 0x0C);
			unsigned int functions = (header & 0x00800000u) != 0 ? 8u : 1u;
			for (unsigned int fn = 0; fn < functions; fn++) {
				uint32_t class_revision = pci_read(
					(uint8_t)bus, (uint8_t)dev, (uint8_t)fn,
					PCI_CLASS_REVISION);
				if ((class_revision >> 8) != AHCI_PCI_CLASS) {
					continue;
				}

				uint32_t command = pci_read(
					(uint8_t)bus, (uint8_t)dev, (uint8_t)fn, PCI_COMMAND);
				pci_write((uint8_t)bus, (uint8_t)dev, (uint8_t)fn,
				          PCI_COMMAND, command | 0x0006u);

				uint32_t bar5 = pci_read(
					(uint8_t)bus, (uint8_t)dev, (uint8_t)fn, PCI_BAR5);
				if ((bar5 & 1u) != 0 || (bar5 & 0xFFFFFFF0u) == 0) {
					continue;
				}
				return (volatile uint32_t *)(uintptr_t)(bar5 & 0xFFFFFFF0u);
			}
		}
	}
	return 0;
}

static uint32_t port_read(unsigned int port, unsigned int reg) {
	return hba[(HBA_PORT_BASE + port * HBA_PORT_STRIDE + reg) / 4];
}

static void port_write(unsigned int port, unsigned int reg, uint32_t value) {
	hba[(HBA_PORT_BASE + port * HBA_PORT_STRIDE + reg) / 4] = value;
}

static int port_wait(unsigned int port, uint32_t mask, uint32_t value) {
	for (uint32_t timeout = 0; timeout < AHCI_TIMEOUT; timeout++) {
		uint32_t current = port_read(port, mask == PORT_CI ? PORT_CI : PORT_TFD);
		if (mask == PORT_CI) {
			if ((current & value) == 0) return 1;
		} else {
			if ((current & PORT_TFD_ERR) != 0) return 0;
			if ((current & mask) == value) return 1;
		}
		io_wait();
	}
	return 0;
}

static int port_stop(unsigned int port) {
	uint32_t command = port_read(port, PORT_CMD);
	command &= ~PORT_CMD_ST;
	port_write(port, PORT_CMD, command);
	for (uint32_t timeout = 0; timeout < AHCI_TIMEOUT; timeout++) {
		if ((port_read(port, PORT_CMD) & PORT_CMD_CR) == 0) break;
		if (timeout + 1 == AHCI_TIMEOUT) return 0;
	}

	command &= ~PORT_CMD_FRE;
	port_write(port, PORT_CMD, command);
	for (uint32_t timeout = 0; timeout < AHCI_TIMEOUT; timeout++) {
		if ((port_read(port, PORT_CMD) & PORT_CMD_FR) == 0) return 1;
	}
	return 0;
}

static int port_start(unsigned int port) {
	for (uint32_t timeout = 0; timeout < AHCI_TIMEOUT; timeout++) {
		uint32_t command = port_read(port, PORT_CMD);
		if ((command & PORT_CMD_CR) == 0) break;
		if (timeout + 1 == AHCI_TIMEOUT) return 0;
	}
	port_write(port, PORT_CMD, port_read(port, PORT_CMD) | PORT_CMD_FRE);
	for (uint32_t timeout = 0; timeout < AHCI_TIMEOUT; timeout++) {
		if ((port_read(port, PORT_CMD) & PORT_CMD_FR) != 0) break;
		if (timeout + 1 == AHCI_TIMEOUT) return 0;
	}
	port_write(port, PORT_CMD, port_read(port, PORT_CMD) | PORT_CMD_ST);
	return 1;
}

static int issue_command(unsigned int port, uint8_t command, uint64_t lba,
                         uint16_t sectors, uint8_t *buffer, int write) {
	ahci_command_header_t *header = &command_lists[port].headers[0];
	ahci_command_table_t *table = &command_tables[port];
	uint8_t *fis = table->fis;

	if (!port_wait(port, PORT_TFD_BSY | PORT_TFD_DRQ, 0)) return 0;
	port_write(port, PORT_IS, 0xFFFFFFFFu);

	for (size_t i = 0; i < sizeof(*table); i++) {
		((uint8_t *)table)[i] = 0;
	}
	header->options = 5u | (write ? (1u << 6) : 0u);
	header->transferred = 0;
	header->table_low = (uint32_t)(uintptr_t)table;
	header->table_high = 0;
	header->options = (header->options & 0xFFFFu) |
		(buffer != 0 ? (1u << 16) : 0u);

	fis[0] = 0x27;
	fis[1] = 0x80;
	fis[2] = command;
	if (command == ATA_CMD_READ_DMA_EXT || command == ATA_CMD_WRITE_DMA_EXT) {
		fis[4] = (uint8_t)lba;
		fis[5] = (uint8_t)(lba >> 8);
		fis[6] = (uint8_t)(lba >> 16);
		fis[7] = 0x40;
		fis[8] = (uint8_t)(lba >> 24);
		fis[9] = (uint8_t)(lba >> 32);
		fis[10] = (uint8_t)(lba >> 40);
		fis[12] = (uint8_t)sectors;
		fis[13] = (uint8_t)(sectors >> 8);
	} else {
		fis[4] = (uint8_t)lba;
		fis[5] = (uint8_t)(lba >> 8);
		fis[6] = (uint8_t)(lba >> 16);
		fis[7] = (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu));
		fis[12] = (uint8_t)sectors;
	}

	if (buffer != 0) {
		table->prdt[0].address_low = (uint32_t)(uintptr_t)buffer;
		table->prdt[0].address_high = 0;
		table->prdt[0].byte_count = (uint32_t)sectors * 512u - 1u;
	}

	__asm__ volatile ("" : : : "memory");
	port_write(port, PORT_CI, 1);
	for (uint32_t timeout = 0; timeout < AHCI_TIMEOUT; timeout++) {
		uint32_t interrupt_status = port_read(port, PORT_IS);
		if ((interrupt_status & PORT_IS_TFES) != 0) return 0;
		if ((port_read(port, PORT_CI) & 1u) == 0) {
			__asm__ volatile ("" : : : "memory");
			return header->transferred == (uint32_t)sectors * 512u;
		}
		io_wait();
	}
	return 0;
}

static uint32_t identify_sectors(const uint8_t *identify) {
	uint32_t word83 = (uint32_t)identify[166] |
		((uint32_t)identify[167] << 8);
	if ((word83 & 0xC000u) == 0x4000u && (word83 & (1u << 10)) != 0) {
		uint32_t sectors = (uint32_t)identify[200] |
			((uint32_t)identify[201] << 8) |
			((uint32_t)identify[202] << 16) |
			((uint32_t)identify[203] << 24);
		return sectors;
	}
	return (uint32_t)identify[120] |
		((uint32_t)identify[121] << 8) |
		((uint32_t)identify[122] << 16) |
		((uint32_t)identify[123] << 24);
}

int ahci_init(void) {
	hba = find_ahci_controller();
	disk_total = 0;
	if (hba == 0) return 0;

	hba[HBA_GHC / 4] |= 1u << 31;
	uint32_t implemented = hba[HBA_PI / 4];
	for (unsigned int port = 0; port < AHCI_MAX_PORTS &&
	     disk_total < AHCI_MAX_DISKS; port++) {
		if ((implemented & (1u << port)) == 0) continue;
		uint32_t ssts = port_read(port, PORT_SSTS);
		if ((ssts & PORT_SSTS_DET_MASK) != PORT_SSTS_DET_PRESENT ||
		    (ssts & PORT_SSTS_IPM_MASK) != PORT_SSTS_IPM_ACTIVE ||
		    port_read(port, PORT_SIG) != PORT_SIG_ATA) {
			continue;
		}
		if (!port_stop(port)) continue;

		command_lists[port].headers[0].table_low =
			(uint32_t)(uintptr_t)&command_tables[port];
		port_write(port, PORT_CLB, (uint32_t)(uintptr_t)&command_lists[port]);
		port_write(port, PORT_CLB + 4, 0);
		port_write(port, PORT_FB, (uint32_t)(uintptr_t)&received_fis[port]);
		port_write(port, PORT_FB + 4, 0);
		port_write(port, PORT_IS, 0xFFFFFFFFu);
		port_write(port, PORT_SERR, 0xFFFFFFFFu);
		if (!port_start(port)) continue;

		if (!issue_command(port, ATA_CMD_IDENTIFY, 0, 1, transfer_buffer, 0)) {
			port_stop(port);
			continue;
		}

		uint32_t sectors = identify_sectors(transfer_buffer);
		if (sectors == 0) {
			port_stop(port);
			continue;
		}
		disk_ports[disk_total] = (uint8_t)port;
		disk_sectors[disk_total] = sectors;
		uint32_t word83 = (uint32_t)transfer_buffer[166] |
			((uint32_t)transfer_buffer[167] << 8);
		disk_flush_supported[disk_total] =
			(word83 & 0xC000u) == 0x4000u &&
			(word83 & (1u << 12)) != 0;
		disk_total++;
	}

	return (int)disk_total;
}

unsigned int ahci_disk_count(void) {
	return disk_total;
}

uint32_t ahci_sector_count(unsigned int disk) {
	return disk < disk_total ? disk_sectors[disk] : 0;
}

static int ahci_transfer(unsigned int disk, uint32_t sector, void *buffer, int write) {
	if (disk >= disk_total || buffer == 0 || sector >= disk_sectors[disk]) {
		return 0;
	}
	uint8_t *bytes = (uint8_t *)buffer;
	if (write) {
		for (unsigned int i = 0; i < sizeof(transfer_buffer); i++) {
			transfer_buffer[i] = bytes[i];
		}
	}
	uint8_t command = sector < ATA_LBA28_SECTORS
		? (write ? ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA)
		: (write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT);
	if (!issue_command(disk_ports[disk], command, sector, 1,
	                   transfer_buffer, write)) {
		return 0;
	}
	if (!write) {
		for (unsigned int i = 0; i < sizeof(transfer_buffer); i++) {
			bytes[i] = transfer_buffer[i];
		}
	}
	return 1;
}

int ahci_read(unsigned int disk, uint32_t sector, void *buffer) {
	return ahci_transfer(disk, sector, buffer, 0);
}

int ahci_write(unsigned int disk, uint32_t sector, const void *buffer) {
	return ahci_transfer(disk, sector, (void *)buffer, 1);
}

int ahci_flush(unsigned int disk) {
	if (disk >= disk_total) return 0;
	if (!disk_flush_supported[disk]) return 1;
	return issue_command(disk_ports[disk], ATA_CMD_FLUSH_CACHE, 0, 0, 0, 0);
}
