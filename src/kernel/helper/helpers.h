#ifndef HELPERS_H
#define HELPERS_H

unsigned int rtcts(unsigned char hour, unsigned char minute, unsigned char second);
void append_uint(char* out, unsigned int value);
void append_padded(char* out, unsigned int value, int width);
unsigned char parse_color(const char* name);

#endif