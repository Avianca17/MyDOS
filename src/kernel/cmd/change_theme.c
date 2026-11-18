#include <cmd/cmd.h>

void cmd_change_theme(char *args, int *cursor_y) {
    if (args[0] == '\0' || change_theme(args) == 0) {
        print("Please choose one of the built-in themes mentioned below:", WHITE, 0, *cursor_y);
        scrl(cursor_y);
        print("default, matrix, ocean, sunset", LIGHT_GREY, 0, *cursor_y);
        scrl(cursor_y);
    } else {
        clear_screen();
        print_logo();
        *cursor_y = 7;
    }
}
