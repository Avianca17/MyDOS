#ifndef FSHELPERS_H
#define FSHELPERS_H

#include "../../../fs/fatfs/source/ff.h"

FRESULT fs_init(void);
FRESULT fs_format(void);
FRESULT make_file(const TCHAR *path);
FRESULT rm_file(const TCHAR *path);
FRESULT list_files(const TCHAR *path, int *cursor_y);
FRESULT make_dir(const TCHAR *path);
void list_drives(int *cursor_y);
FRESULT rm_recursive(const TCHAR *path);
FRESULT cat_file(const TCHAR *path, int *cursor_y);

#endif