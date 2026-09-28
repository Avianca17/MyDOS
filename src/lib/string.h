#ifndef STRING_UTILS_H
#define STRING_UTILS_H

int strlen(const char* s);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
void* memset(void* destination, int value, unsigned int count);
void* memcpy(void* destination, const void* source, unsigned int count);
int memcmp(const void* first, const void* second, unsigned int count);
char* strchr(const char* string, int character);
void itoanum(unsigned int value, char out[32]);

#endif
