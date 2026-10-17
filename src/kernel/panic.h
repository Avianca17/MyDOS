#ifndef PANIC_H
#define PANIC_H

void panic(const char *message) __attribute__((noreturn));
void panic_code(unsigned int code, const char *message) __attribute__((noreturn));
void panic_exception(unsigned char vector, unsigned int error_code, const char *extra_msg) __attribute__((noreturn));

#endif
