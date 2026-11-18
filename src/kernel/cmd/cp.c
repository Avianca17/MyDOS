#include <cmd/cmd.h>

void cmd_cp(char *args, int *cursor_y) {
    cp_file(args, cursor_y);
    scrl(cursor_y);
}
