#include "ff.h"
#include "diskio.h"
#include "kernel/drivers/ahci.h"

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CONTROL 0x3F6
#define ATA_SECONDARY_IO 0x170
#define ATA_SECONDARY_CONTROL 0x376

#define ATA_REG_DATA 0
#define ATA_REG_ERROR 1
#define ATA_REG_SECCOUNT 2
#define ATA_REG_LBA0 3
#define ATA_REG_LBA1 4
#define ATA_REG_LBA2 5
#define ATA_REG_DRIVE 6
#define ATA_REG_STATUS 7
#define ATA_REG_COMMAND 7

#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_FLUSH_CACHE 0xE7

#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_DF 0x20
#define ATA_SR_BSY 0x80

#define ATA_LBA28_SECTORS 0x10000000u
#define ATA_DEVICE_COUNT 4
#define AHCI_FIRST_DRIVE ATA_DEVICE_COUNT
#define AHCI_DRIVE_COUNT 4
#define DISK_COUNT (ATA_DEVICE_COUNT + AHCI_DRIVE_COUNT)

typedef struct {
	unsigned short io_base;
	unsigned short control_base;
	unsigned char slave;
	unsigned char lba_supported;
	unsigned char flush_supported;
	unsigned short cylinders;
	unsigned short heads;
	unsigned short sectors_per_track;
	DSTATUS status;
	LBA_t sector_count;
} ata_device_t;

static ata_device_t ata_devices[ATA_DEVICE_COUNT] = {
	{ ATA_PRIMARY_IO, ATA_PRIMARY_CONTROL, 0, 0, 0, 0, 0, 0, STA_NOINIT, 0 },
	{ ATA_PRIMARY_IO, ATA_PRIMARY_CONTROL, 1, 0, 0, 0, 0, 0, STA_NOINIT, 0 },
	{ ATA_SECONDARY_IO, ATA_SECONDARY_CONTROL, 0, 0, 0, 0, 0, 0, STA_NOINIT, 0 },
	{ ATA_SECONDARY_IO, ATA_SECONDARY_CONTROL, 1, 0, 0, 0, 0, 0, STA_NOINIT, 0 }
};
static DSTATUS ahci_status[AHCI_DRIVE_COUNT] = {
	STA_NOINIT, STA_NOINIT, STA_NOINIT, STA_NOINIT
};
static unsigned char ahci_probed;

static void outb(unsigned short port, unsigned char value) {
	__asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static unsigned char inb(unsigned short port) {
	unsigned char value;
	__asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

static unsigned short inw(unsigned short port) {
	unsigned short value;
	__asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

static void outw(unsigned short port, unsigned short value) {
	__asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static unsigned char ata_read_status(const ata_device_t *device) {
	return inb(device->io_base + ATA_REG_STATUS);
}

static void ata_delay(const ata_device_t *device) {
	inb(device->control_base);
	inb(device->control_base);
	inb(device->control_base);
	inb(device->control_base);
}

static int ata_wait(const ata_device_t *device, unsigned char mask, unsigned char value) {
	unsigned int timeout = 1000000u;

	while (timeout-- != 0u) {
		unsigned char status = ata_read_status(device);

		if (status == 0 || status == 0xFF) {
			return 0;
		}
		if ((status & ATA_SR_BSY) != 0) {
			continue;
		}
		if ((status & (ATA_SR_ERR | ATA_SR_DF)) != 0) {
			return 0;
		}
		if ((status & mask) == value) {
			return 1;
		}
	}

	return 0;
}

static void ata_select(const ata_device_t *device, unsigned char head) {
	outb(device->io_base + ATA_REG_DRIVE,
	     (unsigned char)(0xA0u | (device->slave << 4) | head));
	ata_delay(device);
}

static int ata_identify(ata_device_t *device) {
	unsigned short identify_data[256];
	unsigned char status;

	device->status = STA_NOINIT;
	device->sector_count = 0;
	device->lba_supported = 0;
	device->flush_supported = 0;
	device->cylinders = 0;
	device->heads = 0;
	device->sectors_per_track = 0;

	ata_select(device, 0);
	outb(device->io_base + ATA_REG_SECCOUNT, 0);
	outb(device->io_base + ATA_REG_LBA0, 0);
	outb(device->io_base + ATA_REG_LBA1, 0);
	outb(device->io_base + ATA_REG_LBA2, 0);
	outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

	status = ata_read_status(device);
	if (status == 0 || status == 0xFF || !ata_wait(device, ATA_SR_BSY, 0)) {
		return 0;
	}

	if (inb(device->io_base + ATA_REG_LBA1) != 0 ||
	    inb(device->io_base + ATA_REG_LBA2) != 0 ||
	    !ata_wait(device, ATA_SR_DRQ, ATA_SR_DRQ)) {
		return 0;
	}

	for (unsigned int word = 0; word < 256u; word++) {
		identify_data[word] = inw(device->io_base + ATA_REG_DATA);
	}

	device->lba_supported = (identify_data[49] & (1u << 9)) != 0;
	if (device->lba_supported) {
		LBA_t sectors = ((LBA_t)identify_data[61] << 16) | identify_data[60];
		device->sector_count = sectors > ATA_LBA28_SECTORS
			? ATA_LBA28_SECTORS : sectors;
	} else {
		device->cylinders = identify_data[1];
		device->heads = identify_data[3];
		device->sectors_per_track = identify_data[6];
		if (device->cylinders == 0 || device->heads == 0 ||
		    device->heads > 16 || device->sectors_per_track == 0) {
			return 0;
		}
		device->sector_count = (LBA_t)device->cylinders *
			device->heads * device->sectors_per_track;
	}

	device->flush_supported =
		(identify_data[83] & 0xC000u) == 0x4000u &&
		(identify_data[83] & (1u << 12)) != 0;

	if (device->sector_count == 0) {
		return 0;
	}

	device->status = 0;
	return 1;
}

DSTATUS disk_initialize(BYTE pdrv) {
	if (pdrv < ATA_DEVICE_COUNT) {
		ata_identify(&ata_devices[pdrv]);
		return ata_devices[pdrv].status;
	}
	if (pdrv >= AHCI_FIRST_DRIVE &&
	    pdrv < AHCI_FIRST_DRIVE + AHCI_DRIVE_COUNT) {
		if (!ahci_probed) {
			ahci_init();
			ahci_probed = 1;
			for (unsigned int disk = 0; disk < AHCI_DRIVE_COUNT; disk++) {
				ahci_status[disk] = disk < ahci_disk_count() ? 0 : STA_NOINIT;
			}
		}
		return ahci_status[pdrv - AHCI_FIRST_DRIVE];
	}
	return STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv) {
	if (pdrv < ATA_DEVICE_COUNT) {
		return ata_devices[pdrv].status;
	}
	if (pdrv >= AHCI_FIRST_DRIVE &&
	    pdrv < AHCI_FIRST_DRIVE + AHCI_DRIVE_COUNT) {
		return ahci_status[pdrv - AHCI_FIRST_DRIVE];
	}
	return STA_NOINIT;
}

static DRESULT ata_transfer(ata_device_t *device, BYTE *read_buffer,
                            const BYTE *write_buffer, LBA_t sector,
                            UINT count, int write) {
	if ((device->status & STA_NOINIT) != 0) {
		return RES_NOTRDY;
	}
	if (sector >= device->sector_count ||
	    count > device->sector_count - sector) {
		return RES_PARERR;
	}

	while (count-- != 0u) {
		unsigned char head;

		if (device->lba_supported) {
			head = (unsigned char)(0x40u | ((sector >> 24) & 0x0Fu));
			ata_select(device, head);
			outb(device->io_base + ATA_REG_SECCOUNT, 1);
			outb(device->io_base + ATA_REG_LBA0, (unsigned char)sector);
			outb(device->io_base + ATA_REG_LBA1, (unsigned char)(sector >> 8));
			outb(device->io_base + ATA_REG_LBA2, (unsigned char)(sector >> 16));
		} else {
			unsigned int sectors_per_cylinder =
				(unsigned int)device->heads * device->sectors_per_track;
			unsigned int cylinder = sector / sectors_per_cylinder;
			unsigned int track_sector = sector % sectors_per_cylinder;
			unsigned int head_number = track_sector / device->sectors_per_track;
			unsigned int sector_number =
				(track_sector % device->sectors_per_track) + 1u;

			if (cylinder >= device->cylinders || head_number >= device->heads) {
				return RES_PARERR;
			}
			ata_select(device, (unsigned char)head_number);
			outb(device->io_base + ATA_REG_SECCOUNT, 1);
			outb(device->io_base + ATA_REG_LBA0, (unsigned char)sector_number);
			outb(device->io_base + ATA_REG_LBA1, (unsigned char)cylinder);
			outb(device->io_base + ATA_REG_LBA2, (unsigned char)(cylinder >> 8));
		}

		outb(device->io_base + ATA_REG_COMMAND, write ? ATA_CMD_WRITE : ATA_CMD_READ);
		if (!ata_wait(device, ATA_SR_DRQ, ATA_SR_DRQ)) {
			return RES_ERROR;
		}

		for (unsigned int word = 0; word < 256u; word++) {
			if (write) {
				unsigned short value = (unsigned short)write_buffer[word * 2u]
					| ((unsigned short)write_buffer[word * 2u + 1u] << 8);
				outw(device->io_base + ATA_REG_DATA, value);
			} else {
				unsigned short value = inw(device->io_base + ATA_REG_DATA);
				read_buffer[word * 2u] = (BYTE)value;
				read_buffer[word * 2u + 1u] = (BYTE)(value >> 8);
			}
		}

		if (!ata_wait(device, ATA_SR_BSY, 0)) {
			return RES_ERROR;
		}
		if (write) {
			write_buffer += 512;
		} else {
			read_buffer += 512;
		}
		sector++;
	}

	return RES_OK;
}

static DRESULT secondary_transfer(BYTE pdrv, BYTE *read_buffer,
                                  const BYTE *write_buffer, LBA_t sector,
                                  UINT count, int write) {
	if (pdrv >= AHCI_FIRST_DRIVE &&
	    pdrv < AHCI_FIRST_DRIVE + AHCI_DRIVE_COUNT) {
		unsigned int disk = pdrv - AHCI_FIRST_DRIVE;
		uint32_t sectors = ahci_sector_count(disk);
		if (ahci_status[disk] != 0) return RES_NOTRDY;
		if (sector >= sectors || count > sectors - sector) return RES_PARERR;
		while (count-- != 0u) {
			int ok = write
				? ahci_write(disk, sector, write_buffer)
				: ahci_read(disk, sector, read_buffer);
			if (!ok) return RES_ERROR;
			if (write) write_buffer += 512;
			else read_buffer += 512;
			sector++;
		}
		return RES_OK;
	}
	return RES_PARERR;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
	if (pdrv >= DISK_COUNT || buff == 0 || count == 0) {
		return RES_PARERR;
	}
	if (pdrv < ATA_DEVICE_COUNT) {
		return ata_transfer(&ata_devices[pdrv], buff, 0, sector, count, 0);
	}
	return secondary_transfer(pdrv, buff, 0, sector, count, 0);
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
	if (pdrv >= DISK_COUNT || buff == 0 || count == 0) {
		return RES_PARERR;
	}
	if (pdrv < ATA_DEVICE_COUNT) {
		return ata_transfer(&ata_devices[pdrv], 0, buff, sector, count, 1);
	}
	return secondary_transfer(pdrv, 0, buff, sector, count, 1);
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
	if (pdrv >= DISK_COUNT) {
		return RES_PARERR;
	}
	if ((disk_status(pdrv) & STA_NOINIT) != 0) {
		return RES_NOTRDY;
	}

	if (pdrv >= ATA_DEVICE_COUNT) {
		switch (cmd) {
		case CTRL_SYNC:
			return ahci_flush(pdrv - AHCI_FIRST_DRIVE) ? RES_OK : RES_ERROR;
		case GET_SECTOR_SIZE:
			if (buff == 0) return RES_PARERR;
			*(WORD *)buff = 512;
			return RES_OK;
		case GET_SECTOR_COUNT:
			if (buff == 0) return RES_PARERR;
			*(LBA_t *)buff = ahci_sector_count(pdrv - AHCI_FIRST_DRIVE);
			return RES_OK;
		case GET_BLOCK_SIZE:
			if (buff == 0) return RES_PARERR;
			*(DWORD *)buff = 1;
			return RES_OK;
		default:
			return RES_PARERR;
		}
	}

	ata_device_t *device = &ata_devices[pdrv];
	switch (cmd) {
	case CTRL_SYNC:
		if (device->flush_supported) {
			ata_select(device, 0);
			outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE);
			if (!ata_wait(device, ATA_SR_BSY, 0)) {
				return RES_ERROR;
			}
		}
		return RES_OK;
	case GET_SECTOR_SIZE:
		if (buff == 0) return RES_PARERR;
		*(WORD *)buff = 512;
		return RES_OK;
	case GET_SECTOR_COUNT:
		if (buff == 0) return RES_PARERR;
		*(LBA_t *)buff = device->sector_count;
		return RES_OK;
	case GET_BLOCK_SIZE:
		if (buff == 0) return RES_PARERR;
		*(DWORD *)buff = 1;
		return RES_OK;
	default:
		return RES_PARERR;
	}
}
