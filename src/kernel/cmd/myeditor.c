#include <cmd/cmd.h>

void cmd_myeditor(char *args, int *cursor_y) {
    (void)args;

    myeditor();
    *cursor_y = 0;
}
