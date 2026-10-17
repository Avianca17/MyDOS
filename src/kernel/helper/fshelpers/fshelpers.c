#include "fshelpers.h"
#include "../../../fs/fatfs/source/ff.h"
#include "../../../fs/fatfs/source/diskio.h"
#include "../../drivers/vga.h"
#include "../../kernel.h"

static FATFS fs[FF_VOLUMES];
static BYTE format_work[FF_MAX_SS * 4];
static BYTE active_volume;

FRESULT fs_init(void) {
    FRESULT first_error = FR_NO_FILESYSTEM;
    int mounted_volume = -1;
    int first_ready_volume = -1;
    TCHAR path[3];

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
    MKFS_PARM options = {
        .fmt = FM_FAT32,
        .n_fat = 1,
        .align = 0,
        .n_root = 0,
        .au_size = 0
    };

    TCHAR path[3] = { '0' + active_volume, ':', '\0' };
    FRESULT result = f_mkfs(path, &options, format_work, sizeof(format_work));
    if (result != FR_OK) {
        return result;
    }

    result = f_mount(&fs[active_volume], path, 1);
    if (result != FR_OK) {
        return result;
    }
    return f_chdrive(path);
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
                if (cursor_x == VGA_WIDTH) {
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