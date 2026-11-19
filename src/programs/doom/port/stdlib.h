#ifndef MYDOS_PORT_STDLIB_H
#define MYDOS_PORT_STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *pointer, size_t size);
void free(void *pointer);

int atoi(const char *text);
char *getenv(const char *name);
void exit(int status) __attribute__((noreturn));

static inline int abs(int value) {
    return value < 0 ? -value : value;
}

static inline long labs(long value) {
    return value < 0 ? -value : value;
}

#endif
