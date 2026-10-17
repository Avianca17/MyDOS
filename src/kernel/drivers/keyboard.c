#include <stddef.h>
#include "keyboard.h"
#include "../../lib/string.h"

#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64
#define LEFT_SHIFT  0x2A
#define RIGHT_SHIFT 0x36
#define RIGHT_ALT   0x38

static const char scancode_ascii_default[128] = {
    [0x01] = 0x1B,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x37] = '*', [0x39] = ' ',
    [SCANCODE_F1] = KEY_F1, [SCANCODE_F2] = KEY_F2,
    [SCANCODE_F3] = KEY_F3, [SCANCODE_F4] = KEY_F4,
    [SCANCODE_F5] = KEY_F5, [SCANCODE_F6] = KEY_F6,
    [SCANCODE_F7] = KEY_F7, [SCANCODE_F8] = KEY_F8,
    [SCANCODE_F9] = KEY_F9, [SCANCODE_F10] = KEY_F10,
    [0x4A] = '-', [0x4E] = '+', [0x53] = '.', [0x56] = '<',
    [SCANCODE_F11] = KEY_F11, [SCANCODE_F12] = KEY_F12,
};

static const char scancode_ascii_shift_default[128] = {
    [0x01] = 0x1B,
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
    [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~',
    [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<',
    [0x34] = '>', [0x35] = '?', [0x37] = '*', [0x39] = ' ',
    [SCANCODE_F1] = KEY_SHIFT_F1, [SCANCODE_F2] = KEY_SHIFT_F2,
    [SCANCODE_F3] = KEY_SHIFT_F3, [SCANCODE_F4] = KEY_SHIFT_F4,
    [SCANCODE_F5] = KEY_SHIFT_F5, [SCANCODE_F6] = KEY_SHIFT_F6,
    [SCANCODE_F7] = KEY_SHIFT_F7, [SCANCODE_F8] = KEY_SHIFT_F8,
    [SCANCODE_F9] = KEY_SHIFT_F9, [SCANCODE_F10] = KEY_SHIFT_F10,
    [0x56] = '>',
    [SCANCODE_F11] = KEY_SHIFT_F11, [SCANCODE_F12] = KEY_SHIFT_F12,
};

static const char scancode_ascii_altgr_default[128] = {
};

static const char scancode_ascii_altgr_de[128] = {
    [0x03] = (char)0xFD,
    [0x04] = (char)0xFC,
    [0x08] = '{', [0x09] = '[', [0x0A] = ']', [0x0B] = '}',
    [0x0C] = '\\', [0x10] = '@', [0x1B] = '~',
    [0x32] = (char)0xE6, [0x56] = '|',
};

static char scancode_ascii[128];
static char scancode_ascii_shift[128];
static char scancode_ascii_altgr[128];
static const char *active_layout = "de";
static int keyboard_layout_initialized = 0;

static void keyboard_reset_layouts(void) {
    memcpy(scancode_ascii, scancode_ascii_default, sizeof(scancode_ascii));
    memcpy(scancode_ascii_shift, scancode_ascii_shift_default, sizeof(scancode_ascii_shift));
    memcpy(scancode_ascii_altgr, scancode_ascii_altgr_default, sizeof(scancode_ascii_altgr));
}

static void keyboard_apply_layout(const char *layout_name) {
    keyboard_reset_layouts();

    if (strcmp(layout_name, "us") == 0) {
        active_layout = "us";
        return;
    }

    if (strcmp(layout_name, "de") == 0 || strcmp(layout_name, "default") == 0) {
        active_layout = "de";
        scancode_ascii[0x02] = '1';
        scancode_ascii[0x03] = '2';
        scancode_ascii[0x04] = '3';
        scancode_ascii[0x05] = '4';
        scancode_ascii[0x06] = '5';
        scancode_ascii[0x07] = '6';
        scancode_ascii[0x08] = '7';
        scancode_ascii[0x09] = '8';
        scancode_ascii[0x0A] = '9';
        scancode_ascii[0x0B] = '0';
        scancode_ascii[0x0C] = (char)0xE1;
        scancode_ascii[0x0D] = '\'';
        scancode_ascii[0x15] = 'z';
        scancode_ascii[0x1A] = (char)0x81;
        scancode_ascii[0x1B] = '+';
        scancode_ascii[0x27] = (char)0x94;
        scancode_ascii[0x28] = (char)0x84;
        scancode_ascii[0x29] = '^';
        scancode_ascii[0x2B] = '#';
        scancode_ascii[0x2C] = 'y';
        scancode_ascii[0x35] = '-';
        scancode_ascii[0x56] = '<';

        scancode_ascii_shift[0x02] = '!';
        scancode_ascii_shift[0x03] = '"';
        scancode_ascii_shift[0x04] = (char)0x15;
        scancode_ascii_shift[0x05] = '$';
        scancode_ascii_shift[0x06] = '%';
        scancode_ascii_shift[0x07] = '&';
        scancode_ascii_shift[0x08] = '/';
        scancode_ascii_shift[0x09] = '(';
        scancode_ascii_shift[0x0A] = ')';
        scancode_ascii_shift[0x0B] = '=';
        scancode_ascii_shift[0x0C] = '?';
        scancode_ascii_shift[0x0D] = '`';
        scancode_ascii_shift[0x15] = 'Z';
        scancode_ascii_shift[0x2C] = 'Y';
        scancode_ascii_shift[0x1A] = (char)0x9A;
        scancode_ascii_shift[0x1B] = '*';
        scancode_ascii_shift[0x27] = (char)0x99;
        scancode_ascii_shift[0x28] = (char)0x8E;
        scancode_ascii_shift[0x29] = (char)0xB0;
        scancode_ascii_shift[0x2B] = '\'';
        scancode_ascii_shift[0x35] = '_';
        scancode_ascii_shift[0x33] = ';';
        scancode_ascii_shift[0x34] = ':';
        scancode_ascii_shift[0x56] = '>';
        memcpy(scancode_ascii_altgr, scancode_ascii_altgr_de, sizeof(scancode_ascii_altgr));
        return;
    }

    if (strcmp(layout_name, "fr") == 0) {
        active_layout = "fr";
        scancode_ascii[0x02] = '&';
        scancode_ascii[0x03] = (char)0xE9;
        scancode_ascii[0x04] = '"';
        scancode_ascii[0x05] = '\'';
        scancode_ascii[0x06] = '(';
        scancode_ascii[0x07] = '-';
        scancode_ascii[0x08] = (char)0xE8;
        scancode_ascii[0x09] = '_';
        scancode_ascii[0x0A] = 'c';
        scancode_ascii[0x0B] = 'a';
        scancode_ascii[0x0C] = ')';
        scancode_ascii[0x0D] = '=';
        scancode_ascii[0x10] = 'a';
        scancode_ascii[0x11] = 'z';
        scancode_ascii[0x12] = 'e';
        scancode_ascii[0x13] = 'r';
        scancode_ascii[0x14] = 't';
        scancode_ascii[0x15] = 'y';
        scancode_ascii[0x16] = 'u';
        scancode_ascii[0x17] = 'i';
        scancode_ascii[0x18] = 'o';
        scancode_ascii[0x19] = 'p';
        scancode_ascii[0x1A] = '^';
        scancode_ascii[0x1B] = '$';
        scancode_ascii[0x1E] = 'q';
        scancode_ascii[0x1F] = 's';
        scancode_ascii[0x20] = 'd';
        scancode_ascii[0x21] = 'f';
        scancode_ascii[0x22] = 'g';
        scancode_ascii[0x23] = 'h';
        scancode_ascii[0x24] = 'j';
        scancode_ascii[0x25] = 'k';
        scancode_ascii[0x26] = 'l';
        scancode_ascii[0x27] = 'm';
        scancode_ascii[0x28] = (char)0xF9;
        scancode_ascii[0x29] = '*';
        scancode_ascii[0x2B] = '$';
        scancode_ascii[0x2C] = 'w';
        scancode_ascii[0x2D] = 'x';
        scancode_ascii[0x2E] = 'c';
        scancode_ascii[0x2F] = 'v';
        scancode_ascii[0x30] = 'b';
        scancode_ascii[0x31] = 'n';
        scancode_ascii[0x32] = ',';
        scancode_ascii[0x33] = ';';
        scancode_ascii[0x34] = ':';
        scancode_ascii[0x35] = '!';
        scancode_ascii[0x39] = ' ';
        scancode_ascii[0x56] = '<';
        scancode_ascii_shift[0x03] = '2';
        scancode_ascii_shift[0x04] = '3';
        scancode_ascii_shift[0x07] = '6';
        scancode_ascii_shift[0x08] = '{';
        scancode_ascii_shift[0x09] = '[';
        scancode_ascii_shift[0x0A] = '|';
        scancode_ascii_shift[0x0B] = '@';
        scancode_ascii_shift[0x0C] = ']';
        scancode_ascii_shift[0x0D] = '%';
        scancode_ascii_shift[0x29] = '0';
        scancode_ascii_shift[0x35] = '1';
        scancode_ascii_shift[0x56] = '>';
        return;
    }

    active_layout = "us";
}

int keyboard_load_layout(const char *layout_name) {
    if (layout_name == NULL || layout_name[0] == '\0') {
        return 0;
    }

    if (!keyboard_layout_initialized) {
        keyboard_reset_layouts();
        keyboard_layout_initialized = 1;
    }

    if (strcmp(layout_name, "us") == 0 || strcmp(layout_name, "default") == 0 ||
        strcmp(layout_name, "de") == 0 || strcmp(layout_name, "fr") == 0) {
        keyboard_apply_layout(layout_name);
        return 1;
    }

    return 0;
}

const char *keyboard_get_layout_name(void) {
    return active_layout;
}

static int shift_pressed = 0;
static int altgr_pressed = 0;
static int extended_scancode = 0;

unsigned char inb(unsigned short port) {
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

char keyboard_getchar(void) {
    if (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        unsigned char scancode = inb(KEYBOARD_DATA_PORT);

        if (scancode == 0xE0) {
            extended_scancode = 1;
            return 0;
        }

        if (extended_scancode) {
            extended_scancode = 0;
            if (scancode == RIGHT_ALT) {
                altgr_pressed = 1;
                return 0;
            }
            if (scancode == (RIGHT_ALT | 0x80)) {
                altgr_pressed = 0;
                return 0;
            }
            if (scancode == 0x48) return KEY_ARROW_UP;
            if (scancode == 0x50) return KEY_ARROW_DOWN;
            if (scancode == 0x4B) return KEY_ARROW_LEFT;
            if (scancode == 0x4D) return KEY_ARROW_RIGHT;
            return 0;
        }

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
            if (altgr_pressed && scancode_ascii_altgr[scancode] != 0) {
                return scancode_ascii_altgr[scancode];
            }
            return shift_pressed ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];
        }
    }
    return 0;
}