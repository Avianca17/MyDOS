#include "helpers.h"
#include "kernel.h" // For the color definitions, thats it

unsigned int rtcts(unsigned char hour, unsigned char minute, unsigned char second) {
    return (hour * 3600u) + (minute * 60u) + second;
}
void append_uint(char* out, unsigned int value) {
    char tmp[16];
    int len = 0;

    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0) {
        tmp[len++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (int i = 0; i < len; i++) {
        out[i] = tmp[len - 1 - i];
    }

    out[len] = '\0';
}
void append_padded(char* out, unsigned int value, int width) {
    char tmp[16];
    int len = 0;

    if (value == 0) {
        tmp[len++] = '0';
    }
    else {
        while (value > 0) {
            tmp[len++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }

    while (len < width) {
        tmp[len++] = '0';
    }

    for (int i = 0; i < len; i++) {
        out[i] = tmp[len - 1 - i];
    }

    out[len] = '\0';
}

 unsigned char parse_color(const char* name) {
    if (strcmp(name, "red") == 0) return RED;
    if (strcmp(name, "green") == 0) return GREEN;
    if (strcmp(name, "blue") == 0) return BLUE;
    if (strcmp(name, "cyan") == 0) return CYAN;
    if (strcmp(name, "yellow") == 0) return YELLOW;
    if (strcmp(name, "magenta") == 0) return MAGENTA;
    if (strcmp(name, "white") == 0) return WHITE;
  
    return WHITE; // Default fallback if they typed an unknown color!
}
