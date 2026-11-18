#include <cmd/cmd.h>

void cmd_rbt(char *args, int *cursor_y) {
    (void)args;
    (void)cursor_y;

    __asm__ volatile("movb $0xFE, %%al\n\t"
                     "outb %%al, $0x64\n\t"
                     :
                     :
                     : "al");
}
