#include <mydos.h>
#include "diskio.h"

#define TAR_BLOCK_SIZE 512u
#define INSTALL_PATH_SIZE 256u
#define BIOS_SECTOR_SIZE 512u
#define BIOS_MBR_CODE_SIZE 440u
#define BIOS_TIMESTAMP_OFFSET 218u
#define BIOS_TIMESTAMP_SIZE 6u
#define BIOS_STAGE2_START_LBA 8u
#define BIOS_STAGE2_SECTORS 55u

static FRESULT read_boot_sector(const char *path, uint8_t sector[BIOS_SECTOR_SIZE]) {
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
    result = f_read(&file, sector, BIOS_SECTOR_SIZE, &bytes_read);
    FRESULT close_result = f_close(&file);
    if (result != FR_OK) {
        return result;
    }
    if (bytes_read != BIOS_SECTOR_SIZE) {
        return FR_DISK_ERR;
    }
    return close_result;
}

static FRESULT install_bios_bootloader(BYTE target) {
    uint8_t boot_sector[BIOS_SECTOR_SIZE];
    uint8_t mbr[BIOS_SECTOR_SIZE];
    uint8_t stage2_sector[BIOS_SECTOR_SIZE];
    uint8_t timestamp[BIOS_TIMESTAMP_SIZE];
    FIL stage2_file;
    UINT bytes_read;
    FRESULT result;

    result = read_boot_sector("/boot/limine/limine-bios-mbr.bin", boot_sector);
    if (result != FR_OK) {
        return result;
    }
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        return FR_INVALID_OBJECT;
    }

    result = f_open(&stage2_file, "/boot/limine/limine-bios-stage2.bin", FA_READ);
    if (result != FR_OK) {
        return result;
    }
    if (f_size(&stage2_file) != BIOS_STAGE2_SECTORS * BIOS_SECTOR_SIZE) {
        f_close(&stage2_file);
        return FR_INVALID_OBJECT;
    }

    if (disk_read(target, mbr, 0, 1) != RES_OK) {
        f_close(&stage2_file);
        return FR_DISK_ERR;
    }
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        f_close(&stage2_file);
        return FR_INVALID_OBJECT;
    }

    uint8_t *first_partition = mbr + 446;
    uint32_t partition_start = (uint32_t)first_partition[8] |
        ((uint32_t)first_partition[9] << 8) |
        ((uint32_t)first_partition[10] << 16) |
        ((uint32_t)first_partition[11] << 24);
    if ((first_partition[4] != 0x0B && first_partition[4] != 0x0C) ||
        partition_start < 63u) {
        f_close(&stage2_file);
        return FR_INVALID_OBJECT;
    }
    int active_partition_found = 0;
    for (unsigned int i = 0; i < 4; i++) {
        uint8_t *partition = mbr + 446u + i * 16u;
        uint32_t start = (uint32_t)partition[8] |
            ((uint32_t)partition[9] << 8) |
            ((uint32_t)partition[10] << 16) |
            ((uint32_t)partition[11] << 24);
        if (partition[0] != 0 && partition[0] != 0x80) {
            f_close(&stage2_file);
            return FR_INVALID_OBJECT;
        }
        active_partition_found |= partition[0] == 0x80;
        if (partition[4] != 0 && start < BIOS_STAGE2_START_LBA + BIOS_STAGE2_SECTORS) {
            f_close(&stage2_file);
            return FR_INVALID_OBJECT;
        }
    }

    for (unsigned int sector = 0; sector < BIOS_STAGE2_SECTORS; sector++) {
        bytes_read = 0;
        result = f_read(&stage2_file, stage2_sector, BIOS_SECTOR_SIZE, &bytes_read);
        if (result != FR_OK || bytes_read != BIOS_SECTOR_SIZE) {
            f_close(&stage2_file);
            return result == FR_OK ? FR_DISK_ERR : result;
        }
        if (disk_write(target, stage2_sector, BIOS_STAGE2_START_LBA + sector, 1) != RES_OK) {
            f_close(&stage2_file);
            return FR_DISK_ERR;
        }
    }
    result = f_close(&stage2_file);
    if (result != FR_OK) {
        return result;
    }

    memcpy(timestamp, mbr + BIOS_TIMESTAMP_OFFSET, sizeof(timestamp));
    memcpy(mbr, boot_sector, BIOS_MBR_CODE_SIZE);
    memcpy(mbr + BIOS_TIMESTAMP_OFFSET, timestamp, sizeof(timestamp));
    if (!active_partition_found) {
        mbr[446] = 0x80;
    }
    mbr[446u + 4u] = 0xEF;
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

static void run_installer_command(char *line, const uint8_t *payload, uint32_t payload_size, int *cursor_y) {
    char *arguments = line;
    char *cmd = next_token(&arguments);
    if (cmd == 0) {
        return;
    }

    if (strcmp(cmd, "help") == 0) {
        if (next_token(&arguments) != 0) {
            installer_line("Usage: help", RED, cursor_y);
            return;
        }
        installer_line("Installer commands:", YELLOW, cursor_y);
        installer_line("  clear                  clear the screen", WHITE, cursor_y);
        installer_line("  lsdrv                  list available disks", WHITE, cursor_y);
        installer_line("  mkfat -f 32 <drive>:/  format a disk as FAT32 (erases it)", WHITE, cursor_y);
        installer_line("  mydos-strap <drive>:/  install MyDOS with Limine BIOS/UEFI", WHITE, cursor_y);
        installer_line("  rbt                    reboot", WHITE, cursor_y);
        installer_line("  shtdwn                 powers off", WHITE, cursor_y);
        return;
    }

    if (strcmp(cmd, "lsdrv") == 0) {
        if (next_token(&arguments) != 0) {
            installer_line("Usage: lsdrv", RED, cursor_y);
        } else {
            list_drives(cursor_y);
        }
        return;
    }

    if (strcmp(cmd, "mkfat") == 0) {
        char *format_flag = next_token(&arguments);
        char *filesystem = next_token(&arguments);
        char *path = next_token(&arguments);
        BYTE volume;
        int fat_version = 0;

        if (format_flag == 0 || filesystem == 0 || path == 0 ||
            next_token(&arguments) != 0 ||
            strcmp(format_flag, "-f") != 0 ||
            !parse_drive_root(path, &volume)) {
            installer_line("Usage: mkfat -f <12|16|32> <drive>:/", RED, cursor_y);
            return;
        }

        if (strcmp(filesystem, "12") == 0) {
            fat_version = 12;
        } else if (strcmp(filesystem, "16") == 0) {
            fat_version = 16;
        } else if (strcmp(filesystem, "32") == 0) {
            fat_version = 32;
        } else {
            installer_line("Usage: mkfat -f <12|16|32> <drive>:/", RED, cursor_y);
            return;
        }

        char drive_path[4] = {(char)('0' + volume), ':', '/', '\0'};
        print("Formatting drive ", WHITE, 0, *cursor_y);
        print(drive_path, WHITE, 17, *cursor_y);
        print(" as FAT", WHITE, 20, *cursor_y);
        print(filesystem, WHITE, 27, *cursor_y);
        print("; all data will be erased.", RED, 29, *cursor_y);
        scrl(cursor_y);
        FRESULT result = fs_format_volume_type(volume, fat_version);
        if (result != FR_OK) {
            installer_fatfs_error("mkfat: formatting failed.", result, cursor_y);
        } else {
            print("mkfat: FAT", WHITE, 0, *cursor_y);
            print(filesystem, WHITE, 10, *cursor_y);
            print(" format complete.", WHITE, 12, *cursor_y);
            scrl(cursor_y);
        }
        return;
    }

    if (strcmp(cmd, "mydos-strap") == 0) {
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

        installer_line("Copying MyDOS and Limine files...", WHITE, cursor_y);
        result = unpack_install_package(payload, payload_size);
        if (result != FR_OK) {
            installer_fatfs_error("mydos-strap: could not copy installer files.", result, cursor_y);
            return;
        }

        installer_line("Installing Limine BIOS boot code and UEFI partition type...", WHITE, cursor_y);
        result = install_bios_bootloader(volume);
        if (result != FR_OK) {
            installer_fatfs_error("mydos-strap: could not install BIOS boot code.", result, cursor_y);
            return;
        }

        installer_line("MyDOS installed. Remove the installer media and run rbt.", WHITE, cursor_y);
        return;
    }
    if (strcmp(cmd, "shtdwn") == 0) {
        shutdown();
    }
    if (strcmp(cmd, "rbt") == 0) {
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
    print("Welcome to the MyDOS installer live environment!", WHITE, 0, cursor_y);
    scrl(&cursor_y);
    print("Type 'lsdrv' to list disks, 'help' for installer commands.", WHITE, 0, cursor_y);
    scrl(&cursor_y);
    if (payload == 0 || payload_size == 0) {
        print("Warning: installer package is missing.", RED, 0, cursor_y);
        scrl(&cursor_y);
    }

    char cmd[VGA_MAX_WIDTH - PROMPT_LENGTH + 1];
    int cmd_len = 0;
    int cursor_x = PROMPT_LENGTH;

    print("MyDOS", GREEN, 0, cursor_y);
    print(":", WHITE, 5, cursor_y);
    print("[", RED, 6, cursor_y);
    print("root", LIGHT_RED, 7, cursor_y);
    print("]", RED, 11, cursor_y);
    print("#", WHITE, 12, cursor_y);
    move_cursor(cursor_x, cursor_y);

    for (;;) {
        char key = keyboard_getchar();
        if (key == 0) {
            continue;
        }

        if (key == '\b') {
            if (cursor_x > PROMPT_LENGTH && cmd_len > 0) {
                cmd_len--;
                cmd[cmd_len] = '\0';
                cursor_x--;
                print(" ", WHITE, cursor_x, cursor_y);
                move_cursor(cursor_x, cursor_y);
            }
        } else if (key == '\n' || key == '\r') {
            cmd[cmd_len] = '\0';
            scrl(&cursor_y);

            if (strcmp(cmd, "clear") == 0) {
                clear_screen();
                print_logo();
                cursor_y = 7;
            } else {
                run_installer_command(cmd, payload, payload_size, &cursor_y);
            }

            cmd_len = 0;
            cursor_x = PROMPT_LENGTH;
            print("MyDOS", GREEN, 0, cursor_y);
            print(":", WHITE, 5, cursor_y);
            print("[", RED, 6, cursor_y);
            print("root", LIGHT_RED, 7, cursor_y);
            print("]", RED, 11, cursor_y);
            print("#", WHITE, 12, cursor_y);
            move_cursor(cursor_x, cursor_y);
        } else if (key >= ' ' && key <= '~' &&
                   cmd_len < (int)sizeof(cmd) - 1 &&
                   cursor_x < vga_screen_width()) {
            cmd[cmd_len++] = key;
            cmd[cmd_len] = '\0';
            char character[2] = {key, '\0'};
            print(character, WHITE, cursor_x, cursor_y);
            cursor_x++;
            move_cursor(cursor_x, cursor_y);
        }
    }
}
