#include <cmd/cmd.h>

void cmd_loadkeys(char *args, int *cursor_y) {
    if (strcmp(args, "status") == 0) {
        print("Active keyboard layout: ", WHITE, 0, *cursor_y);
        print(keyboard_get_layout_name(), GREEN, 24, *cursor_y);
        scrl(cursor_y);
    } else if (args[0] == '\0') {
        print("Usage: loadkeys <us|de|fr|status>", RED, 0, *cursor_y);
        scrl(cursor_y);
        print("Available layouts: us, de, fr; use 'status' to check the active layout.",
              WHITE, 0, *cursor_y);
        scrl(cursor_y);
    } else if (keyboard_load_layout(args)) {
        print("Keyboard layout: ", WHITE, 0, *cursor_y);
        print(keyboard_get_layout_name(), GREEN, 19, *cursor_y);
        scrl(cursor_y);
    } else {
        print("loadkeys: unsupported layout. Try us, de, or fr.", RED, 0, *cursor_y);
        scrl(cursor_y);
    }
}
