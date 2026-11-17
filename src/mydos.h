#ifndef MYDOS_H
#define MYDOS_H

#include <stdint.h>
#include "fs/fatfs/source/ff.h"

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MAX_WIDTH 160
#define VGA_MAX_HEIGHT 60
#define VGA_CTRL 0x3D4
#define VGA_DATA 0x3D5
#define PROMPT_LENGTH 14

#define BLACK 0x00
#define BLUE 0x01
#define GREEN 0x02
#define CYAN 0x03
#define RED 0x04
#define MAGENTA 0x05
#define BROWN 0x06
#define LIGHT_GREY 0x07
#define DARK_GREY 0x08
#define LIGHT_BLUE 0x09
#define LIGHT_GREEN 0x0A
#define LIGHT_CYAN 0x0B
#define LIGHT_RED 0x0C
#define LIGHT_MAGENTA 0x0D
#define YELLOW 0x0E
#define WHITE 0x0F

void KernelEntryPoint(unsigned int magic, unsigned int mbi);
void scrl(int *cursor_y);
void shell_run(unsigned int mbi, int cursor_y);
int shell_show_help(char *args, int cursor_y);

void move_cursor(int x, int y);
void print_logo_at(int x, int y);
void print_logo(void);
void print(const char *msg, unsigned char color, int x, int y);
void vga_init_multiboot(unsigned int mbi);
int vga_screen_width(void);
int vga_screen_height(void);
int change_theme(const char *name);
void clear_screen(void);
void scroll_screen(int *cursor_y);
void scroll_screen_region(int first_row, int last_row, int rows);
void print_logo_color(int x, int start_y, unsigned char color);
void draw_pattern(const char *const pattern[], int width, int height,
                  unsigned char color, int x, int y);

unsigned char rtc_read(unsigned char reg);
unsigned char bcdtb(unsigned char value);
void format_rtc_datetime(char out[32]);
void format_rtc_time(char out[32], int format_12h);
void format_rtc_date_only(char out[32]);
void pc_speaker_beep(void);
void fpu_init(void);

FRESULT fs_init(void);
FRESULT fs_format(void);
FRESULT fs_format_volume(BYTE volume);
FRESULT fs_format_volume_type(BYTE volume, int fat_version);
FRESULT make_file(const TCHAR *path);
FRESULT copy_file(const TCHAR *source_path, const TCHAR *destination_path);
void cp_file(char *args, int *cursor_y);
FRESULT move_path(const TCHAR *source_path, const TCHAR *destination_path);
void mv_file(char *args, int *cursor_y);
FRESULT rm_file(const TCHAR *path);
FRESULT list_files(const TCHAR *path, int *cursor_y);
FRESULT make_dir(const TCHAR *path);
void list_drives(int *cursor_y);
FRESULT rm_recursive(const TCHAR *path);
FRESULT cat_file(const TCHAR *path, int *cursor_y);
FRESULT mkfat32(void);

unsigned int rtcts(unsigned char hour, unsigned char minute,
                   unsigned char second);
void append_uint(char *out, unsigned int value);
void append_padded(char *out, unsigned int value, int width);
unsigned char parse_color(const char *name);

void cpuid_vendor(char out[13]);
unsigned int get_total_ram_mb(unsigned int mbi);
FRESULT get_storage_info(unsigned int *used_mb, unsigned int *total_mb);
void shutdown(void);

int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, int n);
void *memset(void *destination, int value, unsigned int count);
void *memcpy(void *destination, const void *source, unsigned int count);
int memcmp(const void *first, const void *second, unsigned int count);
char *strchr(const char *string, int character);
void itoanum(unsigned int value, char out[32]);
void itohex(unsigned int value, char out[11]);

void panic(const char *message) __attribute__((noreturn));
void panic_code(unsigned int code, const char *message) __attribute__((noreturn));
void panic_exception(unsigned char vector, unsigned int error_code,
                     const char *extra_msg) __attribute__((noreturn));

typedef struct gdt_entry {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_middle;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed)) gdt_ptr_t;

void gdt_init(void);

typedef struct idt_entry {
    unsigned short base_low;
    unsigned short sel;
    unsigned char zero;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed)) idt_ptr_t;

typedef struct registers {
    unsigned int gs, fs, es, ds;
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
    unsigned int int_no, err_code;
    unsigned int eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

void idt_init(void);
void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel,
                  unsigned char flags);
void isr_handler(registers_t *regs);

unsigned char io_inb(unsigned short port);
char keyboard_getchar(void);
int keyboard_load_layout(const char *layout_name);
const char *keyboard_get_layout_name(void);

#define SCANCODE_F1 0x3B
#define SCANCODE_F2 0x3C
#define SCANCODE_F3 0x3D
#define SCANCODE_F4 0x3E
#define SCANCODE_F5 0x3F
#define SCANCODE_F6 0x40
#define SCANCODE_F7 0x41
#define SCANCODE_F8 0x42
#define SCANCODE_F9 0x43
#define SCANCODE_F10 0x44
#define SCANCODE_F11 0x57
#define SCANCODE_F12 0x58

#define BOX_DOUBLE_HORIZONTAL ((char)0xCD)
#define BOX_DOUBLE_VERTICAL ((char)0xBA)
#define BOX_DOUBLE_DOWN_RIGHT ((char)0xC9)
#define BOX_DOUBLE_DOWN_LEFT ((char)0xBB)
#define BOX_DOUBLE_UP_RIGHT ((char)0xC8)
#define BOX_DOUBLE_UP_LEFT ((char)0xBC)
#define BOX_DOUBLE_VERTICAL_RIGHT ((char)0xCC)
#define BOX_DOUBLE_VERTICAL_LEFT ((char)0xB9)
#define BOX_DOUBLE_DOWN_HORIZONTAL ((char)0xCB)
#define BOX_DOUBLE_UP_HORIZONTAL ((char)0xCA)
#define BOX_DOUBLE_CROSS ((char)0xCE)

#define BOX_SINGLE_HORIZONTAL ((char)0xC4)
#define BOX_SINGLE_VERTICAL ((char)0xB3)
#define BOX_SINGLE_DOWN_RIGHT ((char)0xDA)
#define BOX_SINGLE_DOWN_LEFT ((char)0xBF)
#define BOX_SINGLE_UP_RIGHT ((char)0xC0)
#define BOX_SINGLE_UP_LEFT ((char)0xD9)
#define BOX_SINGLE_VERTICAL_RIGHT ((char)0xC3)
#define BOX_SINGLE_VERTICAL_LEFT ((char)0xB4)
#define BOX_SINGLE_DOWN_HORIZONTAL ((char)0xC2)
#define BOX_SINGLE_UP_HORIZONTAL ((char)0xC1)
#define BOX_SINGLE_CROSS ((char)0xC5)

#define BOX_D_HORIZ BOX_DOUBLE_HORIZONTAL
#define BOX_D_VERT BOX_DOUBLE_VERTICAL
#define BOX_D_TOPLEFT BOX_DOUBLE_DOWN_RIGHT
#define BOX_D_TOPRIGHT BOX_DOUBLE_DOWN_LEFT
#define BOX_D_BOTLEFT BOX_DOUBLE_UP_RIGHT
#define BOX_D_BOTRIGHT BOX_DOUBLE_UP_LEFT
#define BOX_D_TLEFT BOX_DOUBLE_VERTICAL_RIGHT
#define BOX_D_TRIGHT BOX_DOUBLE_VERTICAL_LEFT
#define BOX_D_TTOP BOX_DOUBLE_DOWN_HORIZONTAL
#define BOX_D_TBOT BOX_DOUBLE_UP_HORIZONTAL
#define BOX_D_CROSS BOX_DOUBLE_CROSS

#define BOX_S_HORIZ BOX_SINGLE_HORIZONTAL
#define BOX_S_VERT BOX_SINGLE_VERTICAL
#define BOX_S_TOPLEFT BOX_SINGLE_DOWN_RIGHT
#define BOX_S_TOPRIGHT BOX_SINGLE_DOWN_LEFT
#define BOX_S_BOTLEFT BOX_SINGLE_UP_RIGHT
#define BOX_S_BOTRIGHT BOX_SINGLE_UP_LEFT
#define BOX_S_TLEFT BOX_SINGLE_VERTICAL_RIGHT
#define BOX_S_TRIGHT BOX_SINGLE_VERTICAL_LEFT
#define BOX_S_TTOP BOX_SINGLE_DOWN_HORIZONTAL
#define BOX_S_TBOT BOX_SINGLE_UP_HORIZONTAL
#define BOX_S_CROSS BOX_SINGLE_CROSS

#define KEY_F1 BOX_DOUBLE_HORIZONTAL
#define KEY_F2 BOX_DOUBLE_VERTICAL
#define KEY_F3 BOX_DOUBLE_DOWN_RIGHT
#define KEY_F4 BOX_DOUBLE_DOWN_LEFT
#define KEY_F5 BOX_DOUBLE_UP_RIGHT
#define KEY_F6 BOX_DOUBLE_UP_LEFT
#define KEY_F7 BOX_DOUBLE_VERTICAL_RIGHT
#define KEY_F8 BOX_DOUBLE_VERTICAL_LEFT
#define KEY_F9 BOX_DOUBLE_DOWN_HORIZONTAL
#define KEY_F10 BOX_DOUBLE_UP_HORIZONTAL
#define KEY_F11 BOX_DOUBLE_CROSS
#define KEY_F12 ((char)0x80)
#define KEY_ARROW_UP ((char)0xF0)
#define KEY_ARROW_DOWN ((char)0xF1)
#define KEY_ARROW_LEFT ((char)0xF2)
#define KEY_ARROW_RIGHT ((char)0xF3)

#define KEY_SHIFT_F1 BOX_SINGLE_HORIZONTAL
#define KEY_SHIFT_F2 BOX_SINGLE_VERTICAL
#define KEY_SHIFT_F3 BOX_SINGLE_DOWN_RIGHT
#define KEY_SHIFT_F4 BOX_SINGLE_DOWN_LEFT
#define KEY_SHIFT_F5 BOX_SINGLE_UP_RIGHT
#define KEY_SHIFT_F6 BOX_SINGLE_UP_LEFT
#define KEY_SHIFT_F7 BOX_SINGLE_VERTICAL_RIGHT
#define KEY_SHIFT_F8 BOX_SINGLE_VERTICAL_LEFT
#define KEY_SHIFT_F9 BOX_SINGLE_DOWN_HORIZONTAL
#define KEY_SHIFT_F10 BOX_SINGLE_UP_HORIZONTAL
#define KEY_SHIFT_F11 BOX_SINGLE_CROSS
#define KEY_SHIFT_F12 ((char)0x80)

int ahci_init(void);
unsigned int ahci_disk_count(void);
uint32_t ahci_sector_count(unsigned int disk);
int ahci_read(unsigned int disk, uint32_t sector, void *buffer);
int ahci_write(unsigned int disk, uint32_t sector, const void *buffer);
int ahci_flush(unsigned int disk);

void myeditor(void);
void main_calc(void);
void installer_main(const uint8_t *payload, uint32_t payload_size);

#endif
