#ifndef MYDOS_PORT_SYS_STAT_H
#define MYDOS_PORT_SYS_STAT_H

#include <sys/types.h>

struct stat {
    off_t st_size;
    mode_t st_mode;
};

int stat(const char *path, struct stat *info);
int fstat(int descriptor, struct stat *info);
int mkdir(const char *path, mode_t mode);

#endif
