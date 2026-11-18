#include <cmd/cmd.h>

void cmd_shtdwn(char *args, int *cursor_y) {
    (void)args;
    (void)cursor_y;

    shutdown();
}
