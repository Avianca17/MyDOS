#ifndef MYDOS_PORT_STRING_H
#define MYDOS_PORT_STRING_H

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t count);
void *memmove(void *destination, const void *source, size_t count);
void *memset(void *destination, int value, size_t count);
int memcmp(const void *first, const void *second, size_t count);

size_t strlen(const char *text);
int strcmp(const char *first, const char *second);
int strncmp(const char *first, const char *second, size_t count);
int strcasecmp(const char *first, const char *second);
int strncasecmp(const char *first, const char *second, size_t count);

char *strcpy(char *destination, const char *source);
char *strncpy(char *destination, const char *source, size_t count);
char *strcat(char *destination, const char *source);
char *strncat(char *destination, const char *source, size_t count);
char *strchr(const char *text, int character);
char *strrchr(const char *text, int character);

#endif
