#include <cmd/cmd.h>

void cmd_calc(char *args, int *cursor_y) {
    (void)args;

    main_calc();
    *cursor_y = 0;
}
