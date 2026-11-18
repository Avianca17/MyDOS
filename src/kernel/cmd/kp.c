#include <cmd/cmd.h>

void cmd_kp(char *args, int *cursor_y) {
    (void)args;
    (void)cursor_y;

    __asm__ volatile("xorl %%eax, %%eax\n\t"
                     "xorl %%edx, %%edx\n\t"
                     "xorl %%ecx, %%ecx\n\t"
                     "divl %%ecx"
                     :
                     :
                     : "eax", "ecx", "edx", "cc");
}
