#include <cmd/cmd.h>

void cmd_clear(char *args, int *cursor_y) {
    clear_screen();
    *cursor_y = 0;

    if (strcmp(args, "-dl") == 0 || strcmp(args, "--draw-logo") == 0) {
        print_logo();
        *cursor_y = 7;
    }
}
