#include "kernel.h"
#include "vga.h"

static void draw_logo_pattern(const char* const pattern[], int width, int height, unsigned char color, int x, int y) {
    volatile unsigned short* vga_buffer = (volatile unsigned short*)VGA_ADDRESS;

    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++) {
            if (pattern[row][column] == ' ') continue;

            unsigned short character = (unsigned short)pattern[row][column];
            int screen_index = (y + row) * VGA_WIDTH + x + column;

            vga_buffer[screen_index] = character | ((unsigned short)color << 8);
        }
    }
}

void move_cursor(int x, int y) {
    unsigned short position = y * VGA_WIDTH + x;

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)0x0F),
          "Nd"((unsigned short)VGA_CTRL)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)(position & 0xFF)),
          "Nd"((unsigned short)VGA_DATA)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)0x0E),
          "Nd"((unsigned short)VGA_CTRL)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)((position >> 8) & 0xFF)),
          "Nd"((unsigned short)VGA_DATA)
    );
}

void print_logo_at(int x, int start_y) {
    static const char* m[] = {
        "M    M",
        "MM  MM",
        "M MM M",
        "M MM M",
        "M    M",
        "M    M"
    };

    static const char* y_pattern[] = {
        "      ",
        "y    y",
        "y    y",
        " yy y ",
        "  yy  ",
        " yy   "
    };

    static const char* d[] = {
        "DDDDD ",
        "D    D",
        "D    D",
        "D    D",
        "D    D",
        "DDDDD "
    };

    static const char* o[] = {
        " OOOO ",
        "O    O",
        "O    O",
        "O    O",
        "O    O",
        " OOOO "
    };

    static const char* s[] = {
        " SSSSS",
        "S     ",
        " SSSS ",
        "     S",
        "     S",
        "SSSSS "
    };

    draw_logo_pattern(m, 6, 6, DARK_GREY, x + 2, start_y);
    draw_logo_pattern(y_pattern, 6, 6, DARK_GREY, x + 9, start_y);
    draw_logo_pattern(d, 6, 6, RED, x + 16, start_y);
    draw_logo_pattern(o, 6, 6, GREEN, x + 23, start_y);
    draw_logo_pattern(s, 6, 6, BLUE, x + 30, start_y);
}

void print_logo(void) {
    print_logo_at(0, 0);
}

void print(const char* msg, unsigned char color, int x, int y) {
    volatile unsigned short* vga_buffer = (volatile unsigned short*)VGA_ADDRESS;

    int offset = y * VGA_WIDTH + x;

    for (int i = 0; msg[i] != '\0'; i++) {
        if (offset >= VGA_WIDTH * VGA_HEIGHT) break;

        vga_buffer[offset] = (unsigned short)msg[i] | ((unsigned short)color << 8);
        offset++;
    }
}

void clear_screen(void) {
    volatile unsigned short* vga_buffer = (volatile unsigned short*)VGA_ADDRESS;

    unsigned short blank = (unsigned short)' ' | ((unsigned short)LIGHT_GREY << 8);

    for (int index = 0; index < VGA_WIDTH * VGA_HEIGHT; index++) {
        vga_buffer[index] = blank;
    }
}

void scroll_screen(int* cursor_y) {
    if (*cursor_y < VGA_HEIGHT) return;

    volatile unsigned short* vga_buffer = (volatile unsigned short*)VGA_ADDRESS;

    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }

    unsigned short blank = (unsigned short)' ' | ((unsigned short)LIGHT_GREY << 8);

    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }

    *cursor_y = VGA_HEIGHT - 1;
}
