#include <setjmp.h>

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"
#include "doom_port.h"

jmp_buf doom_exit_env;
volatile int doom_exit_code;

int doom_main(int argc, char **argv) {
    doom_port_console_init();

    if (setjmp(doom_exit_env)) {
        return doom_exit_code;
    }

    myargc = argc;
    myargv = argv;

    D_DoomMain();

    return 0;
}
