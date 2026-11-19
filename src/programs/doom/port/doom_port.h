#ifndef MYDOS_DOOM_PORT_H
#define MYDOS_DOOM_PORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>

#define DOOM_PORT_KEY_SHIFT 0x100
#define DOOM_PORT_KEY_CTRL  0x101
#define DOOM_PORT_KEY_ALT   0x102

extern jmp_buf doom_exit_env;
extern volatile int doom_exit_code;

void doom_port_console_init(void);
void doom_port_write(const char *text, int length);
void doom_port_fatal(const char *message);
int doom_port_vsnprintf(char *out, size_t size, const char *format, va_list args);

uint64_t doom_port_micros(void);
void doom_port_delay_us(uint32_t us);

int doom_port_key_poll(int *key, int *pressed, int *scancode);
int doom_port_framebuffer_ready(void);
void doom_port_set_palette(const unsigned char *palette);
void doom_port_present(const unsigned char *pixels);

void doom_port_exit(int code) __attribute__((noreturn));

int doom_main(int argc, char **argv);

#endif
