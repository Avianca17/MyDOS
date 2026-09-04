#ifndef STRING_UTILS_H
#define STRING_UTILS_H

int strlen(const char* s);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
void itoanum(unsigned int value, char out[32]);

#endif
