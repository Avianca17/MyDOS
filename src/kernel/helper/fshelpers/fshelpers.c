#include "fshelpers.h"
#include "../../../fs/fatfs/source/ff.h"
#include "../../../fs/fatfs/source/diskio.h"
#include "../../drivers/vga.h"
#include "../../kernel.h"

static FATFS fs;
static BYTE format_work[FF_MAX_SS * 4];

FRESULT fs_init(void) {
    return f_mount(&fs, "", 1);
}

FRESULT fs_format(void) {
    MKFS_PARM options = {
        .fmt = FM_FAT32,
        .n_fat = 1,
        .align = 0,
        .n_root = 0,
        .au_size = 0
    };

    FRESULT result = f_mkfs("", &options, format_work, sizeof(format_work));
    if (result != FR_OK) {
        return result;
    }

    return f_mount(&fs, "", 1);
}

FRESULT make_file(const TCHAR* path) {
    FIL file;
    FRESULT res = f_open(&file, path, FA_CREATE_NEW | FA_WRITE);

    if (res != FR_OK) {
        return res;
    }

    return f_close(&file);
}
FRESULT make_dir(const TCHAR* path) {
    return f_mkdir(path);
}
FRESULT list_files(int* cursor_y) {
    DIR directory;
    FILINFO file_info;
    FRESULT res = f_opendir(&directory, "/");

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
void list_drives(int *cursor_y)
{
    TCHAR label[32];
    DWORD vol_serial;
    TCHAR path[4];

    for (int i = 0; i < FF_VOLUMES; i++)
    {
        // Only show drives that are actually available/initialized
        if (disk_status(i) != 0)
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