#include <cmd/cmd.h>

void cmd_mv(char *args, int *cursor_y) {
    mv_file(args, cursor_y);
    scrl(cursor_y);
}
