#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <mydos.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <setjmp.h>

#include "doom_port.h"

#define PORT_MAX_FILES 12
#define PORT_MALLOC_SIZE (8u * 1024u * 1024u)
#define PIT_CHANNEL2_PORT 0x42
#define PIT_COMMAND_PORT 0x43
#define PIT_SPEAKER_PORT 0x61

struct _MYDOS_FILE {
    int unused;
};

static FILE port_stdin_file;
static FILE port_stdout_file;
static FILE port_stderr_file;

FILE *stdin = &port_stdin_file;
FILE *stdout = &port_stdout_file;
FILE *stderr = &port_stderr_file;

typedef struct {
    size_t size;
    size_t reserved;
} malloc_header_t;

static unsigned char malloc_arena[PORT_MALLOC_SIZE] __attribute__((aligned(16)));
static size_t malloc_used;

typedef struct {
    FIL file;
    int used;
} port_file_t;

static port_file_t open_files[PORT_MAX_FILES];

static int console_x;
static int console_y;

static inline void port_outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t read_tsc_low(void) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return low;
}

static uint32_t cycles_per_us;
static uint32_t timer_last_tsc;
static uint64_t timer_total_us;
static int timer_started;

static void timer_calibrate(void) {
    uint8_t original = io_inb(PIT_SPEAKER_PORT);
    uint32_t total_cycles = 0;
    uint32_t samples = 0;

    port_outb(PIT_SPEAKER_PORT, (original & ~0x02) | 0x01);

    for (int sample = 0; sample < 8; sample++) {
        port_outb(PIT_COMMAND_PORT, 0xB0);
        port_outb(PIT_CHANNEL2_PORT, 11932 & 0xFF);
        port_outb(PIT_CHANNEL2_PORT, (11932 >> 8) & 0xFF);

        uint32_t start = read_tsc_low();
        uint32_t timeout = 50000000u;
        while ((io_inb(PIT_SPEAKER_PORT) & 0x20) == 0 && timeout > 0) {
            timeout--;
        }
        uint32_t end = read_tsc_low();

        if (timeout == 0) {
            break;
        }

        total_cycles += end - start;
        samples++;
    }

    port_outb(PIT_SPEAKER_PORT, original);

    if (samples == 0) {
        cycles_per_us = 1000;
    } else {
        cycles_per_us = (total_cycles / samples) / 10000u;
        if (cycles_per_us == 0) {
            cycles_per_us = 1;
        }
    }
}

uint64_t doom_port_micros(void) {
    if (cycles_per_us == 0) {
        timer_calibrate();
    }

    uint32_t now = read_tsc_low();
    if (!timer_started) {
        timer_started = 1;
        timer_last_tsc = now;
        return 0;
    }

    uint32_t delta = now - timer_last_tsc;
    timer_last_tsc = now;
    timer_total_us += delta / cycles_per_us;
    return timer_total_us;
}

void doom_port_delay_us(uint32_t microseconds) {
    uint64_t start = doom_port_micros();
    while (doom_port_micros() - start < microseconds) {
        __asm__ volatile ("pause");
    }
}

static void console_newline(void) {
    console_x = 0;
    console_y++;
    if (console_y >= vga_screen_height()) {
        scroll_screen_region(0, vga_screen_height() - 1, 1);
        console_y = vga_screen_height() - 1;
    }
}

void doom_port_console_init(void) {
    clear_screen();
    move_cursor(-1, -1);
    console_x = 0;
    console_y = 0;
}

void doom_port_write(const char *text, int length) {
    if (text == 0) {
        return;
    }
    if (length < 0) {
        length = strlen(text);
    }

    for (int index = 0; index < length; index++) {
        char character = text[index];
        if (character == '\n') {
            console_newline();
        } else if (character == '\r') {
            console_x = 0;
        } else if (character == '\t') {
            do {
                char space = ' ';
                print(&space, LIGHT_GREY, console_x, console_y);
                console_x++;
            } while ((console_x & 3) != 0 && console_x < vga_screen_width());
            if (console_x >= vga_screen_width()) {
                console_newline();
            }
        } else if (character == '\b') {
            if (console_x > 0) {
                console_x--;
            }
        } else {
            if (console_x >= vga_screen_width()) {
                console_newline();
            }
            char output[2] = { character, '\0' };
            print(output, LIGHT_GREY, console_x, console_y);
            console_x++;
        }
    }
}

static void append_text(char *out, size_t size, size_t *position,
                        const char *text, int length) {
    for (int index = 0; index < length; index++) {
        if (*position + 1 < size) {
            out[*position] = text[index];
        }
        (*position)++;
    }
}

int doom_port_vsnprintf(char *out, size_t size, const char *format,
                        va_list args) {
    size_t position = 0;

    while (*format != '\0') {
        if (*format != '%') {
            if (position + 1 < size) {
                out[position] = *format;
            }
            position++;
            format++;
            continue;
        }

        format++;
        int left = 0;
        int zero = 0;
        int plus = 0;
        int space = 0;
        int alternate = 0;

        while (*format == '-' || *format == '+' || *format == ' ' ||
               *format == '0' || *format == '#') {
            if (*format == '-') left = 1;
            if (*format == '0') zero = 1;
            if (*format == '+') plus = 1;
            if (*format == ' ') space = 1;
            if (*format == '#') alternate = 1;
            format++;
        }

        int width = 0;
        if (*format == '*') {
            width = va_arg(args, int);
            if (width < 0) {
                left = 1;
                width = -width;
            }
            format++;
        } else {
            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format - '0');
                format++;
            }
        }

        int precision = -1;
        if (*format == '.') {
            format++;
            precision = 0;
            if (*format == '*') {
                precision = va_arg(args, int);
                format++;
            } else {
                while (*format >= '0' && *format <= '9') {
                    precision = precision * 10 + (*format - '0');
                    format++;
                }
            }
        }

        int long_value = 0;
        while (*format == 'l' || *format == 'h' || *format == 'z' ||
               *format == 't' || *format == 'j') {
            if (*format == 'l' || *format == 'z' || *format == 't' ||
                *format == 'j') {
                long_value = 1;
            }
            format++;
        }

        char specifier = *format;
        if (specifier == '\0') {
            break;
        }
        format++;

        if (specifier == '%') {
            if (position + 1 < size) {
                out[position] = '%';
            }
            position++;
            continue;
        }

        if (specifier == 'c') {
            char character = (char)va_arg(args, int);
            int padding = width > 1 ? width - 1 : 0;
            if (!left) {
                for (int index = 0; index < padding; index++) {
                    if (position + 1 < size) out[position] = ' ';
                    position++;
                }
            }
            if (position + 1 < size) out[position] = character;
            position++;
            if (left) {
                for (int index = 0; index < padding; index++) {
                    if (position + 1 < size) out[position] = ' ';
                    position++;
                }
            }
            continue;
        }

        if (specifier == 's') {
            const char *text = va_arg(args, const char *);
            if (text == 0) {
                text = "(null)";
            }
            int length = strlen(text);
            if (precision >= 0 && length > precision) {
                length = precision;
            }
            int padding = width > length ? width - length : 0;
            if (!left) {
                for (int index = 0; index < padding; index++) {
                    if (position + 1 < size) out[position] = ' ';
                    position++;
                }
            }
            append_text(out, size, &position, text, length);
            if (left) {
                for (int index = 0; index < padding; index++) {
                    if (position + 1 < size) out[position] = ' ';
                    position++;
                }
            }
            continue;
        }

        int base = 10;
        int uppercase = 0;
        unsigned long value;
        int negative = 0;

        if (specifier == 'd' || specifier == 'i') {
            long signed_value = long_value ? va_arg(args, long)
                                           : (long)va_arg(args, int);
            if (signed_value < 0) {
                negative = 1;
                value = (unsigned long)(-(signed_value + 1)) + 1;
            } else {
                value = (unsigned long)signed_value;
            }
        } else if (specifier == 'u') {
            value = long_value ? va_arg(args, unsigned long)
                               : (unsigned long)va_arg(args, unsigned int);
        } else if (specifier == 'x' || specifier == 'X') {
            base = 16;
            uppercase = specifier == 'X';
            value = long_value ? va_arg(args, unsigned long)
                               : (unsigned long)va_arg(args, unsigned int);
        } else if (specifier == 'o') {
            base = 8;
            value = long_value ? va_arg(args, unsigned long)
                               : (unsigned long)va_arg(args, unsigned int);
        } else if (specifier == 'p') {
            base = 16;
            alternate = 1;
            value = (unsigned long)va_arg(args, void *);
        } else {
            if (position + 1 < size) out[position] = '%';
            position++;
            if (position + 1 < size) out[position] = specifier;
            position++;
            continue;
        }

        char digits[64];
        int digit_count = 0;
        const char *alphabet = uppercase ? "0123456789ABCDEF"
                                         : "0123456789abcdef";
        do {
            digits[digit_count++] = alphabet[value % (unsigned)base];
            value /= (unsigned)base;
        } while (value != 0);

        if (precision == 0 && digit_count == 1 && digits[0] == '0') {
            digit_count = 0;
        }

        int prefix_length = 0;
        char prefix[4];
        if (negative) {
            prefix[prefix_length++] = '-';
        } else if (plus) {
            prefix[prefix_length++] = '+';
        } else if (space) {
            prefix[prefix_length++] = ' ';
        }
        if (alternate) {
            if (base == 16) {
                prefix[prefix_length++] = '0';
                prefix[prefix_length++] = uppercase ? 'X' : 'x';
            } else if (base == 8 && (digit_count == 0 || digits[digit_count - 1] != '0')) {
                prefix[prefix_length++] = '0';
            }
        }

        int zeros = precision > digit_count ? precision - digit_count : 0;
        int total_length = prefix_length + zeros + digit_count;
        int padding = width > total_length ? width - total_length : 0;

        if (!left) {
            char padding_character = (zero && precision < 0) ? '0' : ' ';
            if (padding_character == '0') {
                for (int index = 0; index < prefix_length; index++) {
                    if (position + 1 < size) out[position] = prefix[index];
                    position++;
                }
                prefix_length = 0;
            }
            for (int index = 0; index < padding; index++) {
                if (position + 1 < size) out[position] = padding_character;
                position++;
            }
        }

        for (int index = 0; index < prefix_length; index++) {
            if (position + 1 < size) out[position] = prefix[index];
            position++;
        }
        for (int index = 0; index < zeros; index++) {
            if (position + 1 < size) out[position] = '0';
            position++;
        }
        for (int index = digit_count - 1; index >= 0; index--) {
            if (position + 1 < size) out[position] = digits[index];
            position++;
        }
        if (left) {
            for (int index = 0; index < padding; index++) {
                if (position + 1 < size) out[position] = ' ';
                position++;
            }
        }
    }

    if (size > 0) {
        size_t terminator = position < size ? position : size - 1;
        out[terminator] = '\0';
    }

    return (int)position;
}

int printf(const char *format, ...) {
    char buffer[1024];
    va_list args;

    va_start(args, format);
    int length = doom_port_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    doom_port_write(buffer, length);
    return length;
}

int vfprintf(FILE *stream, const char *format, va_list args) {
    char buffer[1024];
    int length = doom_port_vsnprintf(buffer, sizeof(buffer), format, args);
    if (stream == stdout || stream == stderr) {
        doom_port_write(buffer, length);
    }
    return length;
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int length = vfprintf(stream, format, args);
    va_end(args);
    return length;
}

int vsnprintf(char *buffer, size_t size, const char *format, va_list args) {
    return doom_port_vsnprintf(buffer, size, format, args);
}

int vsprintf(char *buffer, const char *format, va_list args) {
    return doom_port_vsnprintf(buffer, (size_t)-1, format, args);
}

int snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int length = doom_port_vsnprintf(buffer, size, format, args);
    va_end(args);
    return length;
}

int sprintf(char *buffer, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int length = doom_port_vsnprintf(buffer, (size_t)-1, format, args);
    va_end(args);
    return length;
}

int puts(const char *text) {
    doom_port_write(text, -1);
    doom_port_write("\n", 1);
    return 0;
}

int fputs(const char *text, FILE *stream) {
    if (stream == stdout || stream == stderr) {
        doom_port_write(text, -1);
    }
    return 0;
}

void perror(const char *prefix) {
    if (prefix != 0 && prefix[0] != '\0') {
        doom_port_write(prefix, -1);
        doom_port_write(": ", 2);
    }
    doom_port_write("I/O error\n", -1);
}

FILE *fopen(const char *path, const char *mode) {
    (void)path;
    (void)mode;
    return 0;
}

int fclose(FILE *stream) {
    (void)stream;
    return 0;
}

int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

void setbuf(FILE *stream, char *buffer) {
    (void)stream;
    (void)buffer;
}

int getchar(void) {
    return 0;
}

int fseek(FILE *stream, long offset, int whence) {
    (void)stream;
    (void)offset;
    (void)whence;
    return -1;
}

long ftell(FILE *stream) {
    (void)stream;
    return -1;
}

size_t fread(void *buffer, size_t size, size_t count, FILE *stream) {
    (void)buffer;
    (void)size;
    (void)count;
    (void)stream;
    return 0;
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream) {
    (void)buffer;
    (void)size;
    (void)count;
    (void)stream;
    return 0;
}

int feof(FILE *stream) {
    (void)stream;
    return 1;
}

int fscanf(FILE *stream, const char *format, ...) {
    (void)stream;
    (void)format;
    return EOF;
}

int sscanf(const char *text, const char *format, ...) {
    (void)text;
    (void)format;
    return 0;
}

int remove(const char *path) {
    return unlink(path);
}

char *strcpy(char *destination, const char *source) {
    char *result = destination;
    while ((*destination++ = *source++) != '\0') {
    }
    return result;
}

char *strncpy(char *destination, const char *source, size_t count) {
    char *result = destination;
    while (count > 0 && *source != '\0') {
        *destination++ = *source++;
        count--;
    }
    while (count > 0) {
        *destination++ = '\0';
        count--;
    }
    return result;
}

char *strcat(char *destination, const char *source) {
    char *result = destination;
    while (*destination != '\0') {
        destination++;
    }
    while ((*destination++ = *source++) != '\0') {
    }
    return result;
}

char *strncat(char *destination, const char *source, size_t count) {
    char *result = destination;
    while (*destination != '\0') {
        destination++;
    }
    while (count > 0 && *source != '\0') {
        *destination++ = *source++;
        count--;
    }
    *destination = '\0';
    return result;
}

static int lower_ascii(int character) {
    if (character >= 'A' && character <= 'Z') {
        return character + ('a' - 'A');
    }
    return character;
}

int strcasecmp(const char *first, const char *second) {
    while (*first != '\0' && lower_ascii((unsigned char)*first) ==
                             lower_ascii((unsigned char)*second)) {
        first++;
        second++;
    }
    return lower_ascii((unsigned char)*first) -
           lower_ascii((unsigned char)*second);
}

int strncasecmp(const char *first, const char *second, size_t count) {
    while (count > 0 && *first != '\0' &&
           lower_ascii((unsigned char)*first) ==
               lower_ascii((unsigned char)*second)) {
        first++;
        second++;
        count--;
    }
    if (count == 0) {
        return 0;
    }
    return lower_ascii((unsigned char)*first) -
           lower_ascii((unsigned char)*second);
}

void *memmove(void *destination, const void *source, size_t count) {
    unsigned char *to = destination;
    const unsigned char *from = source;
    if (to < from) {
        for (size_t index = 0; index < count; index++) {
            to[index] = from[index];
        }
    } else if (to > from) {
        for (size_t index = count; index > 0; index--) {
            to[index - 1] = from[index - 1];
        }
    }
    return destination;
}

char *strrchr(const char *text, int character) {
    const char *found = 0;
    do {
        if ((unsigned char)*text == (unsigned char)character) {
            found = text;
        }
    } while (*text++ != '\0');
    return (char *)found;
}

int atoi(const char *text) {
    int value = 0;
    int negative = 0;
    while (*text == ' ' || *text == '\t' || *text == '\n' ||
           *text == '\r') {
        text++;
    }
    if (*text == '-') {
        negative = 1;
        text++;
    } else if (*text == '+') {
        text++;
    }
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (*text - '0');
        text++;
    }
    return negative ? -value : value;
}

char *getenv(const char *name) {
    static char current_directory[] = ".";
    if (name == 0) {
        return 0;
    }
    if (strcmp(name, "HOME") == 0 || strcmp(name, "DOOMWADDIR") == 0) {
        return current_directory;
    }
    return 0;
}

void *malloc(size_t size) {
    if (size == 0) {
        size = 1;
    }
    size = (size + 7u) & ~7u;

    size_t total = sizeof(malloc_header_t) + size;
    if (malloc_used + total > sizeof(malloc_arena)) {
        return 0;
    }

    malloc_header_t *header = (malloc_header_t *)(malloc_arena + malloc_used);
    header->size = size;
    malloc_used += total;
    return header + 1;
}

void free(void *pointer) {
    (void)pointer;
}

void *calloc(size_t count, size_t size) {
    size_t total = count * size;
    void *result = malloc(total);
    if (result != 0) {
        memset(result, 0, total);
    }
    return result;
}

void *realloc(void *pointer, size_t size) {
    if (pointer == 0) {
        return malloc(size);
    }
    if (size == 0) {
        free(pointer);
        return 0;
    }

    malloc_header_t *header = ((malloc_header_t *)pointer) - 1;
    unsigned char *end = (unsigned char *)pointer + header->size;
    size_t rounded = (size + 7u) & ~7u;

    if (end == malloc_arena + malloc_used) {
        size_t used = (size_t)((unsigned char *)header - malloc_arena);
        if (used + sizeof(malloc_header_t) + rounded > sizeof(malloc_arena)) {
            return 0;
        }
        header->size = rounded;
        malloc_used = used + sizeof(malloc_header_t) + rounded;
        return pointer;
    }

    void *replacement = malloc(size);
    if (replacement == 0) {
        return 0;
    }
    size_t copy = header->size < size ? header->size : size;
    memcpy(replacement, pointer, copy);
    return replacement;
}

void exit(int status) {
    doom_port_exit(status);
}

int toupper(int character) {
    if (character >= 'a' && character <= 'z') {
        return character - ('a' - 'A');
    }
    return character;
}

int tolower(int character) {
    return lower_ascii(character);
}

static int descriptor_index(int descriptor) {
    int index = descriptor - 3;
    if (index < 0 || index >= PORT_MAX_FILES || !open_files[index].used) {
        return -1;
    }
    return index;
}

int open(const char *path, int flags, ...) {
    BYTE mode = 0;

    if ((flags & O_RDWR) == O_RDWR) {
        mode = FA_READ | FA_WRITE;
    } else if (flags & O_WRONLY) {
        mode = FA_WRITE;
    } else {
        mode = FA_READ;
    }

    if (flags & O_TRUNC) {
        mode |= FA_CREATE_ALWAYS;
    } else if (flags & O_CREAT) {
        mode |= FA_OPEN_ALWAYS;
    }
    if (flags & O_APPEND) {
        mode |= FA_OPEN_APPEND;
    }

    for (int index = 0; index < PORT_MAX_FILES; index++) {
        if (open_files[index].used) {
            continue;
        }
        if (f_open(&open_files[index].file, path, mode) != FR_OK) {
            return -1;
        }
        open_files[index].used = 1;
        return index + 3;
    }
    return -1;
}

int close(int descriptor) {
    int index = descriptor_index(descriptor);
    if (index < 0) {
        return -1;
    }
    FRESULT result = f_close(&open_files[index].file);
    open_files[index].used = 0;
    return result == FR_OK ? 0 : -1;
}

long read(int descriptor, void *buffer, size_t count) {
    int index = descriptor_index(descriptor);
    if (index < 0) {
        return -1;
    }
    UINT bytes_read = 0;
    if (f_read(&open_files[index].file, buffer, (UINT)count, &bytes_read) != FR_OK) {
        return -1;
    }
    return (long)bytes_read;
}

long write(int descriptor, const void *buffer, size_t count) {
    int index = descriptor_index(descriptor);
    if (index < 0) {
        if (descriptor == 1 || descriptor == 2) {
            doom_port_write((const char *)buffer, (int)count);
            return (long)count;
        }
        return -1;
    }
    UINT bytes_written = 0;
    if (f_write(&open_files[index].file, buffer, (UINT)count, &bytes_written) != FR_OK) {
        return -1;
    }
    return (long)bytes_written;
}

long lseek(int descriptor, long offset, int whence) {
    int index = descriptor_index(descriptor);
    if (index < 0) {
        return -1;
    }

    FSIZE_t position = (FSIZE_t)offset;
    if (whence == SEEK_CUR) {
        position = f_tell(&open_files[index].file) + (FSIZE_t)offset;
    } else if (whence == SEEK_END) {
        position = f_size(&open_files[index].file) + (FSIZE_t)offset;
    }

    if (f_lseek(&open_files[index].file, position) != FR_OK) {
        return -1;
    }
    return (long)f_tell(&open_files[index].file);
}


int access(const char *path, int mode) {
    (void)mode;
    FILINFO info;
    return f_stat(path, &info) == FR_OK ? 0 : -1;
}

int unlink(const char *path) {
    return f_unlink(path) == FR_OK ? 0 : -1;
}

int mkdir(const char *path, mode_t mode) {
    (void)mode;
    return f_mkdir(path) == FR_OK ? 0 : -1;
}

int usleep(unsigned int microseconds) {
    doom_port_delay_us(microseconds);
    return 0;
}

int stat(const char *path, struct stat *info) {
    FILINFO file_info;
    if (info == 0 || f_stat(path, &file_info) != FR_OK) {
        return -1;
    }
    info->st_size = (off_t)file_info.fsize;
    info->st_mode = 0;
    return 0;
}

int fstat(int descriptor, struct stat *info) {
    int index = descriptor_index(descriptor);
    if (index < 0 || info == 0) {
        return -1;
    }
    info->st_size = (off_t)f_size(&open_files[index].file);
    info->st_mode = 0;
    return 0;
}

int doom_port_key_poll(int *key, int *pressed, int *scancode) {
    keyboard_event_t event;
    if (!keyboard_poll_event(&event)) {
        return 0;
    }
    if (key != 0) {
        *key = event.key;
    }
    if (pressed != 0) {
        *pressed = event.pressed;
    }
    if (scancode != 0) {
        *scancode = event.scancode;
    }
    return 1;
}

int doom_port_framebuffer_ready(void) {
    return vga_framebuffer_available();
}

void doom_port_set_palette(const unsigned char *palette) {
    vga_framebuffer_set_indexed_palette(palette);
}

void doom_port_present(const unsigned char *pixels) {
    vga_framebuffer_present_indexed(pixels, 320, 200);
}

void doom_port_fatal(const char *message) {
    doom_port_console_init();
    print("DOOM error:", RED, 0, 0);
    console_y = 1;
    doom_port_write(message, -1);
    doom_port_write("\nPress any key to return.\n", -1);

    int key;
    int pressed;
    int scancode;
    while (!doom_port_key_poll(&key, &pressed, &scancode) || !pressed) {
    }
    doom_port_exit(1);
}

void doom_port_exit(int code) {
    doom_exit_code = code;
    longjmp(doom_exit_env, 1);
    for (;;) {
    }
}
