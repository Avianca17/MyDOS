#include "string.h"

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }

    if (n == 0) return 0;

    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void itoanum(unsigned int value, char out[32]) {
    char tmp[32];
    int i = 0;

    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0) {
        tmp[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (int j = 0; j < i; j++) {
        out[j] = tmp[i - 1 - j];
    }

    out[i] = '\0';
}
