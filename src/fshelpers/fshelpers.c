#include <mydos.h>
#include "../fs/fatfs/source/ff.h"
#include "../fs/fatfs/source/diskio.h"

static FATFS fs[FF_VOLUMES];
static BYTE format_work[FF_MAX_SS * 4];
static BYTE active_volume;

#if FF_MULTI_PARTITION
PARTITION VolToPart[FF_VOLUMES];
#endif

FRESULT mkfat32(void) { return fs_format(); }

FRESULT fs_init(void) {
    FRESULT first_error = FR_NO_FILESYSTEM;
    int mounted_volume = -1;
    int first_ready_volume = -1;
    TCHAR path[3];

#if FF_MULTI_PARTITION
    for (BYTE volume = 0; volume < FF_VOLUMES; volume++) {
        VolToPart[volume].pd = volume;
        VolToPart[volume].pt = 0;
    }
#endif

    for (BYTE volume = 0; volume < FF_VOLUMES; volume++) {
        if (disk_initialize(volume) != 0) {
            continue;
        }
        if (first_ready_volume < 0) {
            first_ready_volume = volume;
        }

        path[0] = '0' + volume;
        path[1] = ':';
        path[2] = '\0';
        FRESULT result = f_mount(&fs[volume], path, 1);
        if (result == FR_OK) {
            if (mounted_volume < 0) {
                mounted_volume = volume;
            }
        } else if (result != FR_NO_FILESYSTEM && first_error == FR_NO_FILESYSTEM) {
            first_error = result;
        }
    }

    if (mounted_volume >= 0) {
        active_volume = (BYTE)mounted_volume;
        path[0] = '0' + active_volume;
        path[1] = ':';
        path[2] = '\0';
        return f_chdrive(path);
    }

    if (first_ready_volume < 0) {
        return FR_NOT_READY;
    }

    active_volume = (BYTE)first_ready_volume;
    return first_error;
}

FRESULT fs_format(void) {
    return fs_format_volume(active_volume);
}

/* Format one FatFs volume / physical disk, then mount it as the active drive. */
FRESULT fs_format_volume(BYTE volume) {
    return fs_format_volume_type(volume, 32);
}

FRESULT fs_format_volume_type(BYTE volume, int fat_version) {
    if (volume >= FF_VOLUMES) {
        return FR_INVALID_DRIVE;
    }

    /* FatFs exposes FAT32 as a dedicated format option; FAT12/FAT16 use the
       generic FAT option and FatFs selects the subtype automatically. */
    MKFS_PARM options = {
        .fmt = (fat_version == 32) ? FM_FAT32 : FM_FAT,
        .n_fat = 1,
        .align = 0,
        .n_root = 0,
        .au_size = 0
    };

    TCHAR path[3] = { '0' + volume, ':', '\0' };
    FRESULT result = f_mkfs(path, &options, format_work, sizeof(format_work));
    if (result != FR_OK) {
        return result;
    }

    result = f_mount(&fs[volume], path, 1);
    if (result != FR_OK) {
        return result;
    }

    result = f_chdrive(path);
    if (result == FR_OK) {
        active_volume = volume;
    }
    return result;
}

FRESULT make_file(const TCHAR* path) {
    FIL file;
    FRESULT res = f_open(&file, path, FA_CREATE_NEW | FA_WRITE);

    if (res != FR_OK) {
        return res;
    }

    return f_close(&file);
}

static FRESULT resolve_destination_path(const TCHAR *source_path, const TCHAR *destination_path, char *resolved_path, unsigned int resolved_path_size, const TCHAR **effective_destination) {
    FILINFO destination_info;
    FRESULT result = f_stat(destination_path, &destination_info);
    if (result != FR_OK && result != FR_NO_FILE && result != FR_NO_PATH) {
        return result;
    }

    *effective_destination = destination_path;
    if (result != FR_OK || !(destination_info.fattrib & AM_DIR)) {
        return FR_OK;
    }

    const char *filename = source_path;
    for (const char *p = source_path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }

    int destination_length = strlen(destination_path);
    int filename_length = strlen(filename);
    int needs_separator = destination_length > 0 &&
                          destination_path[destination_length - 1] != '/' &&
                          destination_path[destination_length - 1] != '\\';
    int total_length = destination_length + needs_separator + filename_length;
    if (filename_length == 0 || total_length >= (int)resolved_path_size) {
        return FR_INVALID_NAME;
    }

    memcpy(resolved_path, destination_path, destination_length);
    int position = destination_length;
    if (needs_separator) {
        resolved_path[position++] = '/';
    }
    memcpy(resolved_path + position, filename, filename_length + 1);
    *effective_destination = resolved_path;
    return FR_OK;
}

FRESULT copy_file(const TCHAR *source_path, const TCHAR *destination_path) {
    FILINFO source_info;
    FRESULT result = f_stat(source_path, &source_info);
    if (result != FR_OK) {
        return result;
    }
    if (source_info.fattrib & AM_DIR) {
        return FR_DENIED;
    }

    char resolved_destination[280];
    const TCHAR *effective_destination;
    result = resolve_destination_path(source_path, destination_path, resolved_destination, sizeof(resolved_destination), &effective_destination);
    if (result != FR_OK) {
        return result;
    }

    FIL source;
    result = f_open(&source, source_path, FA_READ);
    if (result != FR_OK) {
        return result;
    }

    FIL destination;
    result = f_open(&destination, effective_destination, FA_CREATE_NEW | FA_WRITE);
    if (result != FR_OK) {
        f_close(&source);
        return result;
    }

    BYTE buffer[512];
    UINT bytes_read;
    do {
        result = f_read(&source, buffer, sizeof(buffer), &bytes_read);
        if (result != FR_OK || bytes_read == 0) {
            break;
        }

        UINT bytes_written;
        result = f_write(&destination, buffer, bytes_read, &bytes_written);
        if (result == FR_OK && bytes_written != bytes_read) {
            result = FR_DISK_ERR;
        }
    } while (result == FR_OK);

    FRESULT close_result = f_close(&destination);
    if (result == FR_OK) {
        result = close_result;
    }

    close_result = f_close(&source);
    if (result == FR_OK) {
        result = close_result;
    }

    if (result != FR_OK) {
        FRESULT cleanup_result = f_unlink(effective_destination);
        if (cleanup_result != FR_OK && cleanup_result != FR_NO_FILE) {
            return cleanup_result;
        }
    }

    return result;
}

static int parse_two_paths(char *args, char **source, char **destination) {
    *source = args;
    while (**source == ' ') {
        (*source)++;
    }

    char *separator = *source;
    while (*separator != '\0' && *separator != ' ') {
        separator++;
    }
    if (*separator == '\0') {
        return 0;
    }

    *separator++ = '\0';
    while (*separator == ' ') {
        separator++;
    }
    if (*separator == '\0') {
        return 0;
    }

    *destination = separator;
    while (*separator != '\0' && *separator != ' ') {
        separator++;
    }
    if (*separator != '\0') {
        *separator++ = '\0';
        while (*separator == ' ') {
            separator++;
        }
        if (*separator != '\0') {
            return 0;
        }
    }

    return 1;
}

void cp_file(char *args, int *cursor_y) {
    int recursive = 0;
    char *p = args;

    while (*p == ' ') {
        p++;
    }

    if (strncmp(p, "-r", 2) == 0 &&
        (p[2] == '\0' || p[2] == ' ')) {
        recursive = 1;
        p += 2;
    } else if (strncmp(p, "--recursive", 11) == 0 && (p[11] == '\0' || p[11] == ' ')) {
        recursive = 1;
        p += 11;
    }

    char *source;
    char *destination;
    if (!parse_two_paths(p, &source, &destination)) {
        print("Usage: cp [-r|--recursive] <source> <destination>", RED, 0, *cursor_y);
        return;
    }

    FILINFO source_info;
    FRESULT result = f_stat(source, &source_info);
    if (result == FR_OK && (source_info.fattrib & AM_DIR) && !recursive) {
        print("cp: omitting directory; use -r", RED, 0, *cursor_y);
        return;
    }

    result = recursive
        ? copy_path(source, destination)
        : copy_file(source, destination);

    if (result == FR_OK) {
        print(recursive ? "cp: Path copied." : "cp: File copied.", WHITE, 0, *cursor_y);
    } else if (result == FR_NO_FILE || result == FR_NO_PATH) {
        print("cp: Source path not found.", RED, 0, *cursor_y);
    } else if (result == FR_EXIST) {
        print("cp: Destination already exists.", RED, 0, *cursor_y);
    } else if (result == FR_DENIED) {
        print("cp: Cannot copy a directory into itself.", RED, 0, *cursor_y);
    } else {
        print("cp: Could not copy path.", RED, 0, *cursor_y);
    }
}

FRESULT move_path(const TCHAR *source_path, const TCHAR *destination_path) {
    FILINFO source_info;
    FRESULT result = f_stat(source_path, &source_info);
    if (result != FR_OK) {
        return result;
    }

    char resolved_destination[280];
    const TCHAR *effective_destination;
    result = resolve_destination_path(source_path, destination_path, resolved_destination, sizeof(resolved_destination), &effective_destination);
    if (result != FR_OK) {
        return result;
    }

    if (source_info.fattrib & AM_DIR) {
        int source_length = strlen(source_path);
        int destination_length = strlen(effective_destination);
        if (destination_length > source_length &&
            strncmp(effective_destination, source_path, source_length) == 0 &&
            (source_path[source_length - 1] == '/' ||
             source_path[source_length - 1] == '\\' ||
             effective_destination[source_length] == '/' ||
             effective_destination[source_length] == '\\')) {
            return FR_DENIED;
        }
    }

    return f_rename(source_path, effective_destination);
}

void mv_file(char *args, int *cursor_y) {
    char *source;
    char *destination;
    if (!parse_two_paths(args, &source, &destination)) {
        print("Usage: mv <source> <destination>", RED, 0, *cursor_y);
        return;
    }

    FRESULT result = move_path(source, destination);
    if (result == FR_OK) {
        print("mv: Path moved.", GREEN, 0, *cursor_y);
    } else if (result == FR_NO_FILE || result == FR_NO_PATH) {
        print("mv: Source path not found.", RED, 0, *cursor_y);
    } else if (result == FR_EXIST) {
        print("mv: Destination already exists.", RED, 0, *cursor_y);
    } else if (result == FR_DENIED) {
        print("mv: Cannot move a directory into itself.", RED, 0, *cursor_y);
    } else {
        print("mv: Could not move path.", RED, 0, *cursor_y);
    }
}

FRESULT make_dir(const TCHAR* path) {
    return f_mkdir(path);
}
FRESULT list_files(const TCHAR *path, int *cursor_y) {
    DIR directory;
    FILINFO file_info;
    FRESULT res = f_opendir(&directory, path);

    if (res != FR_OK) {
        return res;
    }

    for (;;) {
        res = f_readdir(&directory, &file_info);
        if (res != FR_OK || file_info.fname[0] == '\0') {
            break;
        }

        print(file_info.fname,
              (file_info.fattrib & AM_DIR) != 0 ? GREEN : WHITE,
              0,
              *cursor_y);
        scrl(cursor_y);
    }

    f_closedir(&directory);
    return res;
}

static void list_mbr_partitions(BYTE drive, int *cursor_y) {
    BYTE sector[FF_MAX_SS];
    DRESULT result = disk_read(drive, sector, 0, 1);

    if (result != RES_OK) {
        print("Could not read partition table.", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        print("No MBR partition table detected.", LIGHT_GREY, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    int partition_count = 0;
    for (int i = 0; i < 4; i++) {
        const BYTE *entry = &sector[446 + i * 16];
        BYTE type = entry[4];
        DWORD start_lba = (DWORD)entry[8] |
                          ((DWORD)entry[9] << 8) |
                          ((DWORD)entry[10] << 16) |
                          ((DWORD)entry[11] << 24);
        DWORD sector_count = (DWORD)entry[12] |
                             ((DWORD)entry[13] << 8) |
                             ((DWORD)entry[14] << 16) |
                             ((DWORD)entry[15] << 24);

        if (type == 0 || sector_count == 0) {
            continue;
        }

        if (type == 0xEE) {
            print("GPT detected; GPT partitions are not listed.", YELLOW, 0, *cursor_y);
            scrl(cursor_y);
            return;
        }

        char partition_number[2] = {(char)('1' + i), '\0'};
        char type_code[11];
        char start_text[32];
        char size_text[32];
        itohex(type, type_code);
        itoanum(start_lba, start_text);
        itoanum(sector_count, size_text);

        print("  partition ", LIGHT_CYAN, 0, *cursor_y);
        print(partition_number, LIGHT_CYAN, 12, *cursor_y);
        print(" type ", WHITE, 14, *cursor_y);
        print(type_code, WHITE, 20, *cursor_y);
        scrl(cursor_y);
        print("    start LBA: ", LIGHT_GREY, 0, *cursor_y);
        print(start_text, WHITE, 15, *cursor_y);
        print("  sectors: ", LIGHT_GREY, 32, *cursor_y);
        print(size_text, WHITE, 43, *cursor_y);
        scrl(cursor_y);
        partition_count++;
    }

    if (partition_count == 0) {
        print("  No MBR partitions found.", LIGHT_GREY, 0, *cursor_y);
        scrl(cursor_y);
    }
}

void list_drives(int *cursor_y)
{
    TCHAR label[32];
    DWORD vol_serial;
    TCHAR path[4];

    for (int i = 0; i < FF_VOLUMES; i++)
    {
        DSTATUS status = disk_status((BYTE)i);
        if ((status & STA_NOINIT) != 0)
        {
            status = disk_initialize((BYTE)i);
        }
        if (status != 0)
        {
            continue;
        }

        path[0] = '0' + i;
        path[1] = ':';
        path[2] = '\0';

        FRESULT res = f_getlabel(path, label, &vol_serial);

        print("drive ", WHITE, 0, *cursor_y);
        char drive_num_str[2] = {'0' + i, '\0'};
        print(drive_num_str, WHITE, 6, *cursor_y);
        print(": ", WHITE, 7, *cursor_y);

        if (res == FR_OK && label[0] != '\0')
        {
            print(label, WHITE, 11, *cursor_y);
        }
        else
        {
            print("Unnamed Drive", WHITE, 11, *cursor_y);
        }

        print("available", GREEN, 71, *cursor_y);
        scrl(cursor_y);
        list_mbr_partitions((BYTE)i, cursor_y);
    }
}
FRESULT rm_file(const TCHAR *path) {
    return f_unlink(path);
}

static void join_path(char *dest, int dest_size, const char *dir, const char *file) {
    int i = 0;
    while (*dir && i < dest_size - 1) {
        dest[i++] = *dir++;
    }
    if (i > 0 && dest[i - 1] != '/' && i < dest_size - 1) {
        dest[i++] = '/';
    }
    while (*file && i < dest_size - 1) {
        dest[i++] = *file++;
    }
    dest[i] = '\0';
}

FRESULT copy_path(const TCHAR *source_path, const TCHAR *destination_path) {
    FILINFO source_info;
    FRESULT result = f_stat(source_path, &source_info);
    if (result != FR_OK) {
        return result;
    }

    if (!(source_info.fattrib & AM_DIR)) {
        return copy_file(source_path, destination_path);
    }

    char resolved_destination[280];
    const TCHAR *effective_destination;
    result = resolve_destination_path(
        source_path,
        destination_path,
        resolved_destination,
        sizeof(resolved_destination),
        &effective_destination
    );
    if (result != FR_OK) {
        return result;
    }

    int source_length = strlen(source_path);
    int destination_length = strlen(effective_destination);

    if (destination_length > source_length &&
        strncmp(effective_destination, source_path, source_length) == 0 &&
        (source_path[source_length - 1] == '/' ||
         source_path[source_length - 1] == '\\' ||
         effective_destination[source_length] == '/' ||
         effective_destination[source_length] == '\\')) {
        return FR_DENIED;
    }

    result = f_mkdir(effective_destination);
    if (result != FR_OK && result != FR_EXIST) {
        return result;
    }

    DIR directory;
    result = f_opendir(&directory, source_path);
    if (result != FR_OK) {
        return result;
    }

    FILINFO file_info;
    char source_child[280];
    char destination_child[280];

    for (;;) {
        result = f_readdir(&directory, &file_info);
        if (result != FR_OK || file_info.fname[0] == '\0') {
            break;
        }

        if (file_info.fname[0] == '.') {
            if (file_info.fname[1] == '\0') {
                continue;
            }
            if (file_info.fname[1] == '.' && file_info.fname[2] == '\0') {
                continue;
            }
        }

        join_path(source_child, sizeof(source_child), source_path, file_info.fname);
        join_path(destination_child, sizeof(destination_child),
                  effective_destination, file_info.fname);

        result = copy_path(source_child, destination_child);
        if (result != FR_OK) {
            break;
        }
    }

    f_closedir(&directory);
    return result;
}

FRESULT rm_recursive(const TCHAR *path) {
    FILINFO fno;
    FRESULT res = f_stat(path, &fno);
    if (res != FR_OK) {
        return res;
    }

    if (!(fno.fattrib & AM_DIR)) {
        return f_unlink(path);
    }

    DIR dir;
    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        return res;
    }

    char subpath[280];

    for (;;) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == '\0') {
            break;
        }

        if (fno.fname[0] == '.') {
            if (fno.fname[1] == '\0') {
                continue;
            }
            if (fno.fname[1] == '.' && fno.fname[2] == '\0') {
                continue;
            }
        }

        join_path(subpath, sizeof(subpath), path, fno.fname);

        res = rm_recursive(subpath);
        if (res != FR_OK) {
            break;
        }
    }

    f_closedir(&dir);

    if (res != FR_OK) {
        return res;
    }

    return f_unlink(path);
}
FRESULT cat_file(const TCHAR *path, int *cursor_y) {
    FIL file;
    FRESULT res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        return res;
    }

    char buffer[128];
    UINT bytes_read;
    int cursor_x = 0;

    while (f_read(&file, buffer, sizeof(buffer), &bytes_read) == FR_OK && bytes_read > 0) {
        for (UINT i = 0; i < bytes_read; i++) {
            char ch = buffer[i];

            if (ch == '\r') {
                continue;
            } else if (ch == '\n') {
                cursor_x = 0;
                scrl(cursor_y);
            } else {
                if (cursor_x == vga_screen_width()) {
                    cursor_x = 0;
                    scrl(cursor_y);
                }

                char str[2] = {ch, '\0'};
                print(str, WHITE, cursor_x, *cursor_y);
                cursor_x++;
            }
        }
    }

    if (cursor_x > 0) {
        scrl(cursor_y);
    }

    f_close(&file);
    return FR_OK;
}
