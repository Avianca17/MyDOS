#include <mydos.h>
#include "diskio.h"

#define TAR_BLOCK_SIZE 512u
#define INSTALL_PATH_SIZE 256u
#define BIOS_SECTOR_SIZE 512u
#define BIOS_MBR_CODE_SIZE 440u
#define BIOS_CORE_START_LBA 1u

/* Write one little-endian value into the GRUB boot image fields. */
static void store_le16(uint8_t *destination, uint16_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
}

static void store_le64(uint8_t *destination, uint64_t value) {
    for (unsigned int i = 0; i < 8; i++) {
        destination[i] = (uint8_t)(value >> (i * 8));
    }
}

/* Read exactly one 512-byte BIOS GRUB image file from the installed volume. */
static FRESULT read_boot_image(const char *path, uint8_t image[BIOS_SECTOR_SIZE]) {
    FIL file;
    UINT bytes_read = 0;
    FRESULT result = f_open(&file, path, FA_READ);
    if (result != FR_OK) {
        return result;
    }
    if (f_size(&file) != BIOS_SECTOR_SIZE) {
        f_close(&file);
        return FR_INVALID_OBJECT;
    }
    result = f_read(&file, image, BIOS_SECTOR_SIZE, &bytes_read);
    FRESULT close_result = f_close(&file);
    if (result != FR_OK) {
        return result;
    }
    if (bytes_read != BIOS_SECTOR_SIZE) {
        return FR_DISK_ERR;
    }
    return close_result;
}

/*
 * Install BIOS GRUB into the MBR and the 1 MiB gap before the FAT partition.
 * The boot sector and core image are shipped as files in the in-memory
 * installer package, so the installer does not need an ISO9660 driver.
 */
static FRESULT install_bios_bootloader(BYTE target) {
    uint8_t boot_sector[BIOS_SECTOR_SIZE];
    uint8_t mbr[BIOS_SECTOR_SIZE];
    uint8_t core_sector[BIOS_SECTOR_SIZE];
    FIL core_file;
    UINT bytes_read;
    FRESULT result;

    /* GRUB's first stage starts loading core.img from sector 1 of the disk. */
    result = read_boot_image("/boot/grub/i386-pc/boot.img", boot_sector);
    if (result != FR_OK) {
        return result;
    }

    result = f_open(&core_file, "/boot/grub/core.img", FA_READ);
    if (result != FR_OK) {
        return result;
    }
    uint32_t core_size = (uint32_t)f_size(&core_file);
    uint32_t core_sectors = (core_size + BIOS_SECTOR_SIZE - 1u) / BIOS_SECTOR_SIZE;
    /* One sector is read by boot.img; diskboot's list reads all remaining sectors. */
    if (core_size < BIOS_SECTOR_SIZE * 2u || core_sectors > 0xFFFFu ||
        core_sectors >= 2048u) {
        f_close(&core_file);
        return FR_INVALID_OBJECT;
    }

    /*
     * Match GRUB's BIOS setup blocklist layout in diskboot.img. The list is
     * stored backward from byte 512: first the remaining core sectors, then
     * a zero-length terminator. GRUB loads those sectors at segment 0x820.
     */
    memset(core_sector, 0, sizeof(core_sector));
    bytes_read = 0;
    result = f_read(&core_file, core_sector, BIOS_SECTOR_SIZE, &bytes_read);
    if (result != FR_OK || bytes_read != BIOS_SECTOR_SIZE) {
        f_close(&core_file);
        return result == FR_OK ? FR_DISK_ERR : result;
    }
    store_le64(core_sector + BIOS_SECTOR_SIZE - 12u, BIOS_CORE_START_LBA + 1u);
    store_le16(core_sector + BIOS_SECTOR_SIZE - 4u, (uint16_t)(core_sectors - 1u));
    store_le16(core_sector + BIOS_SECTOR_SIZE - 2u, 0x0820u);
    memset(core_sector + BIOS_SECTOR_SIZE - 24u, 0, 12u);
    if (disk_write(target, core_sector, BIOS_CORE_START_LBA, 1) != RES_OK) {
        f_close(&core_file);
        return FR_DISK_ERR;
    }

    /* Copy/pad each remaining core sector into the reserved post-MBR gap. */
    for (uint32_t sector = 1; sector < core_sectors; sector++) {
        memset(core_sector, 0, sizeof(core_sector));
        UINT wanted = core_size - sector * BIOS_SECTOR_SIZE;
        if (wanted > BIOS_SECTOR_SIZE) {
            wanted = BIOS_SECTOR_SIZE;
        }
        bytes_read = 0;
        result = f_read(&core_file, core_sector, wanted, &bytes_read);
        if (result != FR_OK || bytes_read != wanted) {
            f_close(&core_file);
            return result == FR_OK ? FR_DISK_ERR : result;
        }
        if (disk_write(target, core_sector, BIOS_CORE_START_LBA + sector, 1) != RES_OK) {
            f_close(&core_file);
            return FR_DISK_ERR;
        }
    }
    result = f_close(&core_file);
    if (result != FR_OK) {
        return result;
    }

    /* Point boot.img at core.img and let BIOS pass through the boot drive. */
    store_le64(boot_sector + 0x5Cu, BIOS_CORE_START_LBA);
    boot_sector[0x64] = 0xFF;
    if (disk_read(target, mbr, 0, 1) != RES_OK) {
        return FR_DISK_ERR;
    }
    /* Preserve the generated MBR disk signature, partition table and 0x55AA. */
    memcpy(mbr, boot_sector, BIOS_MBR_CODE_SIZE);
    if (disk_write(target, mbr, 0, 1) != RES_OK ||
        disk_ioctl(target, CTRL_SYNC, 0) != RES_OK) {
        return FR_DISK_ERR;
    }
    return FR_OK;
}

static int tar_parse_octal(const uint8_t *field, unsigned int length,
                           uint32_t *value) {
    uint32_t parsed = 0;
    int found_digit = 0;
    int ended = 0;

    /* Tar stores file sizes and checksums as fixed-width octal text fields. */
    for (unsigned int i = 0; i < length; i++) {
        uint8_t character = field[i];
        if (character == '\0' || character == ' ') {
            if (found_digit) {
                ended = 1;
            }
            continue;
        }
        if (character < '0' || character > '7' || ended) {
            return 0;
        }
        if (parsed > (0xFFFFFFFFu - (uint32_t)(character - '0')) / 8u) {
            return 0;
        }
        parsed = parsed * 8u + (uint32_t)(character - '0');
        found_digit = 1;
    }

    *value = parsed;
    return 1;
}

static int tar_header_valid(const uint8_t *header) {
    uint32_t expected;
    uint32_t actual = 0;

    if (!tar_parse_octal(header + 148, 8, &expected)) {
        return 0;
    }
    /* The checksum bytes count as spaces while recomputing the header sum. */
    for (unsigned int i = 0; i < TAR_BLOCK_SIZE; i++) {
        actual += (i >= 148 && i < 156) ? (uint8_t)' ' : header[i];
    }
    return actual == expected;
}

static unsigned int tar_field_length(const uint8_t *field, unsigned int limit) {
    unsigned int length = 0;
    while (length < limit && field[length] != '\0') {
        length++;
    }
    return length;
}

static int tar_make_path(const uint8_t *header, char path[INSTALL_PATH_SIZE]) {
    unsigned int name_length = tar_field_length(header, 100);
    unsigned int prefix_length = tar_field_length(header + 345, 155);
    unsigned int position = 0;

    if (name_length == 0) {
        return 0;
    }
    /* USTAR may split long names between its prefix and name fields. */
    if (prefix_length > 0) {
        if (prefix_length + 1 + name_length >= INSTALL_PATH_SIZE) {
            return 0;
        }
        memcpy(path, header + 345, prefix_length);
        position = prefix_length;
        path[position++] = '/';
    } else if (name_length >= INSTALL_PATH_SIZE) {
        return 0;
    }
    memcpy(path + position, header, name_length);
    position += name_length;
    path[position] = '\0';

    /* Only relative paths inside the package are accepted; reject traversal. */
    if (path[0] == '/' || path[0] == '\\') {
        return 0;
    }
    for (unsigned int start = 0, i = 0; i <= position; i++) {
        if (path[i] == '\\') {
            return 0;
        }
        if (path[i] == '/' || path[i] == '\0') {
            unsigned int component_length = i - start;
            if (component_length == 2 && path[start] == '.' && path[start + 1] == '.') {
                return 0;
            }
            if (component_length == 1 && path[start] == '.') {
                return 0;
            }
            start = i + 1;
        }
    }
    return 1;
}

static FRESULT make_path_directories(const char *path, int include_last) {
    char directory[INSTALL_PATH_SIZE];
    unsigned int length = (unsigned int)strlen(path);
    if (length == 0 || length >= sizeof(directory)) {
        return FR_INVALID_NAME;
    }
    memcpy(directory, path, length + 1);
    if (directory[0] != '/') {
        return FR_INVALID_NAME;
    }

    /* Create each parent directory in order; FR_EXIST means it is already there. */
    for (unsigned int i = 1; i <= length; i++) {
        if (directory[i] == '/' || directory[i] == '\0') {
            char saved = directory[i];
            if (saved == '\0' && !include_last) {
                break;
            }
            directory[i] = '\0';
            if (directory[1] != '\0') {
                FRESULT result = f_mkdir(directory);
                if (result != FR_OK && result != FR_EXIST) {
                    return result;
                }
            }
            directory[i] = saved;
        }
    }
    return FR_OK;
}

static FRESULT tar_copy_file(const uint8_t *data, uint32_t size,
                             const char *path) {
    FRESULT result = make_path_directories(path, 0);
    if (result != FR_OK) {
        return result;
    }

    FIL file;
    result = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (result != FR_OK) {
        return result;
    }

    /* Stream in small chunks so the installer does not need a large RAM buffer. */
    uint32_t offset = 0;
    while (offset < size) {
        UINT chunk = size - offset > 512u ? 512u : (UINT)(size - offset);
        UINT written = 0;
        result = f_write(&file, data + offset, chunk, &written);
        if (result != FR_OK || written != chunk) {
            if (result == FR_OK) {
                result = FR_DISK_ERR;
            }
            break;
        }
        offset += chunk;
    }

    FRESULT close_result = f_close(&file);
    return result == FR_OK ? close_result : result;
}

static FRESULT unpack_install_package(const uint8_t *payload,
                                      uint32_t payload_size) {
    uint32_t offset = 0;
    unsigned int copied_files = 0;

    /* Walk 512-byte tar headers and check every size against the module bounds. */
    while (offset <= payload_size && payload_size - offset >= TAR_BLOCK_SIZE) {
        const uint8_t *header = payload + offset;
        int empty = 1;
        for (unsigned int i = 0; i < TAR_BLOCK_SIZE; i++) {
            if (header[i] != 0) {
                empty = 0;
                break;
            }
        }
        if (empty) {
            break;
        }
        if (!tar_header_valid(header)) {
            return FR_INVALID_OBJECT;
        }

        uint32_t file_size;
        if (!tar_parse_octal(header + 124, 12, &file_size) ||
            file_size > 0xFFFFFFFFu - (TAR_BLOCK_SIZE - 1u)) {
            return FR_INVALID_OBJECT;
        }
        uint32_t data_offset = offset + TAR_BLOCK_SIZE;
        uint32_t padded_size = (file_size + TAR_BLOCK_SIZE - 1u) & ~(TAR_BLOCK_SIZE - 1u);
        if (data_offset < offset || padded_size > payload_size - data_offset) {
            return FR_INVALID_OBJECT;
        }

        char relative_path[INSTALL_PATH_SIZE];
        if (!tar_make_path(header, relative_path)) {
            return FR_INVALID_NAME;
        }
        char path[INSTALL_PATH_SIZE];
        unsigned int path_length = (unsigned int)strlen(relative_path);
        if (path_length + 2 >= sizeof(path)) {
            return FR_INVALID_NAME;
        }
        path[0] = '/';
        memcpy(path + 1, relative_path, path_length + 1);

        /* Extract ordinary files and directories; links and special files are ignored. */
        uint8_t type = header[156];
        FRESULT result = FR_OK;
        if (type == '5') {
            unsigned int length = (unsigned int)strlen(path);
            while (length > 1 && path[length - 1] == '/') {
                path[--length] = '\0';
            }
            result = make_path_directories(path, 1);
        } else if (type == '\0' || type == '0') {
            result = tar_copy_file(payload + data_offset, file_size, path);
            if (result == FR_OK) {
                copied_files++;
            }
        }
        if (result != FR_OK) {
            return result;
        }

        offset = data_offset + padded_size;
    }

    return copied_files > 0 ? FR_OK : FR_NO_FILE;
}

static FATFS strap_filesystems[FF_VOLUMES];

#define INSTALLER_PROMPT "mydos-install> "
#define INSTALLER_PROMPT_LENGTH 15

static void installer_line(const char *text, unsigned char color, int *cursor_y) {
    print(text, color, 0, *cursor_y);
    scrl(cursor_y);
}

static void installer_fatfs_error(const char *message, FRESULT result,
                                  int *cursor_y) {
    char code[32];
    itoanum((unsigned int)result, code);
    installer_line(message, RED, cursor_y);
    print("FATFS error code: ", LIGHT_GREY, 0, *cursor_y);
    print(code, WHITE, 18, *cursor_y);
    scrl(cursor_y);
}

static char *next_token(char **input) {
    while (**input == ' ') {
        (*input)++;
    }
    if (**input == '\0') {
        return 0;
    }

    char *token = *input;
    while (**input != '\0' && **input != ' ') {
        (*input)++;
    }
    if (**input != '\0') {
        **input = '\0';
        (*input)++;
    }
    return token;
}

static int parse_drive_root(const char *path, BYTE *volume) {
    if (strlen(path) != 3 || path[0] < '0' || path[0] > '9' ||
        path[1] != ':' || path[2] != '/') {
        return 0;
    }

    BYTE drive = (BYTE)(path[0] - '0');
    if (drive >= FF_VOLUMES) {
        return 0;
    }
    *volume = drive;
    return 1;
}

static FRESULT mount_fat32_volume(BYTE volume) {
    TCHAR drive[3] = {(TCHAR)('0' + volume), ':', '\0'};
    if (disk_initialize(volume) != 0) {
        return FR_NOT_READY;
    }

    FRESULT result = f_mount(&strap_filesystems[volume], drive, 1);
    if (result != FR_OK) {
        return result;
    }
    if (strap_filesystems[volume].fs_type != FS_FAT32) {
        return FR_NO_FILESYSTEM;
    }
    return f_chdrive(drive);
}

static void run_installer_command(char *line, const uint8_t *payload,
                                  uint32_t payload_size, int *cursor_y) {
    char *arguments = line;
    char *command = next_token(&arguments);
    if (command == 0) {
        return;
    }

    if (strcmp(command, "help") == 0) {
        if (next_token(&arguments) != 0) {
            installer_line("Usage: help", RED, cursor_y);
            return;
        }
        installer_line("Installer commands:", YELLOW, cursor_y);
        installer_line("  lsdrv                  list available disks", WHITE, cursor_y);
        installer_line("  mkfat -f 32 <drive>:/  format a disk as FAT32 (erases it)", WHITE, cursor_y);
        installer_line("  mydos-strap <drive>:/  copy MyDOS and install BIOS boot code", WHITE, cursor_y);
        installer_line("  rbt                    reboot", WHITE, cursor_y);
        return;
    }

    if (strcmp(command, "lsdrv") == 0) {
        if (next_token(&arguments) != 0) {
            installer_line("Usage: lsdrv", RED, cursor_y);
        } else {
            list_drives(cursor_y);
        }
        return;
    }

    if (strcmp(command, "mkfat") == 0) {
        char *format_flag = next_token(&arguments);
        char *filesystem = next_token(&arguments);
        char *path = next_token(&arguments);
        BYTE volume;
        if (format_flag == 0 || filesystem == 0 || path == 0 ||
            next_token(&arguments) != 0 ||
            strcmp(format_flag, "-f") != 0 || strcmp(filesystem, "32") != 0 ||
            !parse_drive_root(path, &volume)) {
            installer_line("Usage: mkfat -f 32 <drive>:/", RED, cursor_y);
            return;
        }

        char drive_path[4] = {(char)('0' + volume), ':', '/', '\0'};
        print("Formatting drive ", YELLOW, 0, *cursor_y);
        print(drive_path, YELLOW, 17, *cursor_y);
        print(" as FAT32; all data will be erased.", YELLOW, 20, *cursor_y);
        scrl(cursor_y);
        FRESULT result = fs_format_volume(volume);
        if (result != FR_OK) {
            installer_fatfs_error("mkfat: formatting failed.", result, cursor_y);
        } else {
            installer_line("mkfat: FAT32 format complete.", GREEN, cursor_y);
        }
        return;
    }

    if (strcmp(command, "mydos-strap") == 0) {
        char *path = next_token(&arguments);
        BYTE volume;
        if (path == 0 || next_token(&arguments) != 0 ||
            !parse_drive_root(path, &volume)) {
            installer_line("Usage: mydos-strap <drive>:/", RED, cursor_y);
            return;
        }
        if (payload == 0 || payload_size == 0) {
            installer_line("mydos-strap: installer package is missing.", RED, cursor_y);
            return;
        }

        FRESULT result = mount_fat32_volume(volume);
        if (result != FR_OK) {
            if (result == FR_NO_FILESYSTEM) {
                installer_line("mydos-strap: target is not FAT32; run mkfat first.", RED, cursor_y);
            } else {
                installer_fatfs_error("mydos-strap: could not mount target.", result, cursor_y);
            }
            return;
        }

        installer_line("Copying MyDOS and GRUB files...", WHITE, cursor_y);
        result = unpack_install_package(payload, payload_size);
        if (result != FR_OK) {
            installer_fatfs_error("mydos-strap: could not copy installer files.", result, cursor_y);
            return;
        }

        installer_line("Installing BIOS boot code...", WHITE, cursor_y);
        result = install_bios_bootloader(volume);
        if (result != FR_OK) {
            installer_fatfs_error("mydos-strap: could not install BIOS boot code.", result, cursor_y);
            return;
        }

        installer_line("MyDOS installed. Remove the installer media and run rbt.", GREEN, cursor_y);
        return;
    }

    if (strcmp(command, "rbt") == 0) {
        if (next_token(&arguments) != 0) {
            installer_line("Usage: rbt", RED, cursor_y);
            return;
        }
        __asm__ volatile("movb $0xFE, %%al\n\t"
                         "outb %%al, $0x64\n\t"
                         :
                         :
                         : "al");
        return;
    }

    installer_line("Unknown installer command. Run 'help'.", RED, cursor_y);
}

void installer_main(const uint8_t *payload, uint32_t payload_size) {
    clear_screen();
    print_logo();
    int cursor_y = 7;
    installer_line("MyDOS command-line installer", YELLOW, &cursor_y);
    if (payload == 0 || payload_size == 0) {
        installer_line("Warning: installer package is missing.", RED, &cursor_y);
    }
    installer_line("Run 'help' to see installer commands.", LIGHT_GREY, &cursor_y);

    char line[64];
    int line_length = 0;
    int cursor_x = INSTALLER_PROMPT_LENGTH;
    print(INSTALLER_PROMPT, GREEN, 0, cursor_y);
    move_cursor(cursor_x, cursor_y);

    for (;;) {
        char key = keyboard_getchar();
        if (key == 0) {
            continue;
        }
        if (key == '\b') {
            if (line_length > 0) {
                line[--line_length] = '\0';
                cursor_x--;
                print(" ", WHITE, cursor_x, cursor_y);
                move_cursor(cursor_x, cursor_y);
            }
            continue;
        }
        if (key == '\n' || key == '\r') {
            line[line_length] = '\0';
            scrl(&cursor_y);
            run_installer_command(line, payload, payload_size, &cursor_y);
            line_length = 0;
            cursor_x = INSTALLER_PROMPT_LENGTH;
            print(INSTALLER_PROMPT, GREEN, 0, cursor_y);
            move_cursor(cursor_x, cursor_y);
            continue;
        }
        if (key >= ' ' && key <= '~' && line_length < (int)sizeof(line) - 1) {
            line[line_length++] = key;
            line[line_length] = '\0';
            char character[2] = {key, '\0'};
            print(character, WHITE, cursor_x, cursor_y);
            cursor_x++;
            move_cursor(cursor_x, cursor_y);
        }
    }
}
