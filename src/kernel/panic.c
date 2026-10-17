#include "panic.h"
#include "kernel.h"
#include "drivers/vga.h"
#include "drivers/pc_speaker.h"
#include "lib/string.h"

static const char* const exclamation_pattern[] = {
    "####",
    "####",
    "####",
    " ## ",
    "    ",
    " ## "
};

static const char* const exception_names[32] = {
    [0x00] = "Divide-by-Zero Error (#DE)",
    [0x01] = "Debug Exception (#DB)",
    [0x02] = "Non-Maskable Interrupt (#NMI)",
    [0x03] = "Breakpoint Exception (#BP)",
    [0x04] = "Overflow Exception (#OF)",
    [0x05] = "Bound Range Exceeded (#BR)",
    [0x06] = "Invalid Opcode (#UD)",
    [0x07] = "Device Not Available (#NM)",
    [0x08] = "Double Fault (#DF)",
    [0x09] = "Coprocessor Segment Overrun",
    [0x0A] = "Invalid TSS (#TS)",
    [0x0B] = "Segment Not Present (#NP)",
    [0x0C] = "Stack-Segment Fault (#SS)",
    [0x0D] = "General Protection Fault (#GP)",
    [0x0E] = "Page Fault (#PF)",
    [0x0F] = "Reserved Exception",
    [0x10] = "x87 FPU Floating-Point Error (#MF)",
    [0x11] = "Alignment Check (#AC)",
    [0x12] = "Machine Check (#MC)",
    [0x13] = "SIMD Floating-Point Exception (#XM)",
    [0x14] = "Virtualization Exception (#VE)",
    [0x15] = "Control Protection Exception (#CP)",
    [0x16] = "Reserved Exception",
    [0x17] = "Reserved Exception",
    [0x18] = "Reserved Exception",
    [0x19] = "Reserved Exception",
    [0x1A] = "Reserved Exception",
    [0x1B] = "Reserved Exception",
    [0x1C] = "Hypervisor Injection Exception",
    [0x1D] = "VMM Communication Exception",
    [0x1E] = "Security Exception",
    [0x1F] = "Reserved Exception"
};

static void print_centered(const char* prefix, const char* body, int y) {
    int prefix_len = prefix ? strlen(prefix) : 0;
    int body_len = body ? strlen(body) : 0;
    int total_len = prefix_len + body_len;

    int x = (80 - total_len) / 2;
    if (x < 1) x = 1;

    if (prefix && prefix[0] != '\0') {
        print(prefix, WHITE, x, y);
    }
    if (body && body[0] != '\0') {
        print(body, WHITE, x + prefix_len, y);
    }
}

static void panic_render(unsigned int code, const char *exception_name, const char *message) __attribute__((noreturn));

static void panic_render(unsigned int code, const char *exception_name, const char *message) {
    __asm__ volatile("cli");

    pc_speaker_beep();
    clear_screen();
    print_logo_color(2, 2, WHITE);
    draw_pattern(exclamation_pattern, 4, 6, WHITE, 41, 2);

    print_centered("", "KERNEL PANIC!", 10);
    print_centered("", "Please reboot your computer.", 12);

    char hex_buf[11];
    itohex(code, hex_buf);
    print_centered("Code: ", hex_buf, 14);

    if (exception_name && exception_name[0] != '\0') {
        print_centered("Exception: ", exception_name, 15);
    }
    if (message && message[0] != '\0') {
        print_centered("Error: ", message, 17);
    }
    move_cursor(80, 25);
    while (1) {
        __asm__ volatile("hlt");
    }
}

void panic(const char *message) {
    panic_code(0x00000001, message);
}

void panic_code(unsigned int code, const char *message) {
    panic_render(code, 0, message);
}

void panic_exception(unsigned char vector, unsigned int error_code, const char *extra_msg) {
    const char *name = (vector < 32 && exception_names[vector]) ? exception_names[vector] : "Unknown CPU Exception";
    unsigned int code = (error_code != 0) ? error_code : (unsigned int)vector;
    panic_render(code, name, extra_msg);
}
