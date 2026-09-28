#include "ff.h"
#include "diskio.h"

#define ATA_DATA 0x1F0
#define ATA_SECCOUNT0 0x1F2
#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5
#define ATA_HDDEVSEL 0x1F6
#define ATA_COMMAND 0x1F7
#define ATA_STATUS 0x1F7
#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_DF 0x20
#define ATA_SR_BSY 0x80

static DSTATUS drive_status = STA_NOINIT;
static LBA_t drive_sector_count;

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

static void ata_delay(void) {
	inb(ATA_STATUS); inb(ATA_STATUS); inb(ATA_STATUS); inb(ATA_STATUS);
}

static int ata_wait(unsigned char mask, unsigned char value) {
	unsigned int timeout = 1000000u;
	while (timeout-- != 0u) {
		unsigned char status = inb(ATA_STATUS);
		if ((status & (ATA_SR_ERR | ATA_SR_DF)) != 0u) return 0;
		if ((status & mask) == value) return 1;
	}
	return 0;
}

static int ata_identify(void) {
	unsigned short identify_data[256];
	unsigned char status;

	outb(ATA_HDDEVSEL, 0xE0);
	ata_delay();
	outb(ATA_SECCOUNT0, 0);
	outb(ATA_LBA0, 0);
	outb(ATA_LBA1, 0);
	outb(ATA_LBA2, 0);
	outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

	status = inb(ATA_STATUS);
	if (status == 0 || !ata_wait(ATA_SR_BSY, 0)) return 0;
	if (!ata_wait(ATA_SR_DRQ, ATA_SR_DRQ)) return 0;

	for (unsigned int word = 0; word < 256u; word++) {
		identify_data[word] = inw(ATA_DATA);
	}

	drive_sector_count = ((LBA_t)identify_data[61] << 16) | identify_data[60];
	return drive_sector_count != 0;
}

DSTATUS disk_initialize(BYTE pdrv) {
	if (pdrv != 0) return STA_NOINIT;
	drive_sector_count = 0;
	drive_status = ata_identify() ? 0 : STA_NOINIT;
	return drive_status;
}

DSTATUS disk_status(BYTE pdrv) {
	return pdrv == 0 ? drive_status : STA_NOINIT;
}

static DRESULT ata_transfer(BYTE* read_buffer, const BYTE* write_buffer, LBA_t sector, UINT count, int write) {
	while (count-- != 0u) {
		if ((sector & 0xF0000000u) != 0u) return RES_PARERR;
		outb(ATA_HDDEVSEL, (unsigned char)(0xE0u | ((sector >> 24) & 0x0Fu)));
		ata_delay();
		outb(ATA_SECCOUNT0, 1);
		outb(ATA_LBA0, (unsigned char)sector);
		outb(ATA_LBA1, (unsigned char)(sector >> 8));
		outb(ATA_LBA2, (unsigned char)(sector >> 16));
		outb(ATA_COMMAND, write ? ATA_CMD_WRITE : ATA_CMD_READ);
		if (!ata_wait(ATA_SR_BSY | ATA_SR_DRQ, ATA_SR_DRQ)) return RES_ERROR;

		for (unsigned int word = 0; word < 256u; word++) {
			if (write) {
				unsigned short value = (unsigned short)write_buffer[word * 2u]
					| ((unsigned short)write_buffer[word * 2u + 1u] << 8);
				__asm__ volatile ("outw %0, %1" : : "a"(value), "d"((unsigned short)ATA_DATA));
			} else {
				unsigned short value = inw(ATA_DATA);
				read_buffer[word * 2u] = (BYTE)value;
				read_buffer[word * 2u + 1u] = (BYTE)(value >> 8);
			}
		}
		if (write) {
			outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
			if (!ata_wait(ATA_SR_BSY, 0)) return RES_ERROR;
			write_buffer += 512;
		} else {
			read_buffer += 512;
		}
		sector++;
	}
	return RES_OK;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
	if (pdrv != 0 || drive_status != 0 || buff == 0 || count == 0) return RES_PARERR;
	return ata_transfer(buff, 0, sector, count, 0);
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
	if (pdrv != 0 || drive_status != 0 || buff == 0 || count == 0) return RES_PARERR;
	return ata_transfer(0, buff, sector, count, 1);
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
	if (pdrv != 0 || drive_status != 0) return RES_PARERR;
	if (cmd == CTRL_SYNC) return RES_OK;
	if (cmd == GET_SECTOR_SIZE) {
		*(WORD*)buff = 512;
		return RES_OK;
	}
	if (cmd == GET_SECTOR_COUNT) {
		*(LBA_t*)buff = drive_sector_count;
		return RES_OK;
	}
	if (cmd == GET_BLOCK_SIZE) {
		*(DWORD*)buff = 1;
		return RES_OK;
	}
	return RES_PARERR;
}

