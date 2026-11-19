#ifndef MYDOS_PORT_SETJMP_H
#define MYDOS_PORT_SETJMP_H

typedef void *jmp_buf[5];

#define setjmp(environment) __builtin_setjmp((void **)(environment))
#define longjmp(environment, value) __builtin_longjmp((void **)(environment), 1)

#endif
