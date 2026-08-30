#include "keyboard.h"

#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64
#define LEFT_SHIFT  0x2A
#define RIGHT_SHIFT 0x36

static const char scancode_ascii[128] = {
    [0x01] = 0x1B,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = (char)0xE1,
    [0x0D] = '\'', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'z', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = (char)0x81,
    [0x1B] = '+', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = (char)0x94,
    [0x28] = (char)0x84, [0x29] = '^', [0x2B] = '#',
    [0x2C] = 'y', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '-', [0x37] = '*', [0x39] = ' ',
    [0x4A] = '-', [0x4E] = '+', [0x53] = '.', [0x56] = '<',
};

static const char scancode_ascii_shift[128] = {
    [0x01] = 0x1B,
    [0x02] = '!', [0x03] = '"', [0x04] = (char)0x15,
    [0x05] = '$', [0x06] = '%', [0x07] = '&', [0x08] = '/',
    [0x09] = '(', [0x0A] = ')', [0x0B] = '=', [0x0C] = '?',
    [0x0D] = '`', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Z', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1A] = (char)0x9A,
    [0x1B] = '*', [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = (char)0x99,
    [0x28] = (char)0x8E, [0x29] = (char)0xF8,
    [0x2B] = '\'',
    [0x2C] = 'Y', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = ';',
    [0x34] = ':', [0x35] = '_', [0x37] = '*', [0x39] = ' ',
    [0x56] = '>',
};

static int shift_pressed = 0;

unsigned char inb(unsigned short port) {
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

char keyboard_getchar(void) {
    if (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        unsigned char scancode = inb(KEYBOARD_DATA_PORT);

        if (scancode == LEFT_SHIFT || scancode == RIGHT_SHIFT) {
            shift_pressed = 1;
            return 0;
        }

        if (scancode == (LEFT_SHIFT | 0x80) || scancode == (RIGHT_SHIFT | 0x80)) {
            shift_pressed = 0;
            return 0;
        }

        if (scancode & 0x80) {
            return 0;
        }

        if (scancode < 128) {
            return shift_pressed ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];
        }
    }
    return 0;
}