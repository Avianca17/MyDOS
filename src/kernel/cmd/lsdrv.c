#include <cmd/cmd.h>

void cmd_lsdrv(char *args, int *cursor_y) {
    if (args[0] != '\0') {
        print("Usage: lsdrv", RED, 0, *cursor_y);
    } else {
        list_drives(cursor_y);
    }
}
