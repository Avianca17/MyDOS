#include <cmd/cmd.h>

void cmd_shlogo(char *args, int *cursor_y) {
    (void)args;

    print_logo_at(0, *cursor_y);
    *cursor_y += 6;
    scrl(cursor_y);
}
