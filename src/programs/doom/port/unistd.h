#ifndef MYDOS_PORT_UNISTD_H
#define MYDOS_PORT_UNISTD_H

#include <stddef.h>

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

int access(const char *path, int mode);
int close(int descriptor);
long read(int descriptor, void *buffer, size_t count);
long write(int descriptor, const void *buffer, size_t count);
long lseek(int descriptor, long offset, int whence);
int unlink(const char *path);
int usleep(unsigned int microseconds);

#endif
