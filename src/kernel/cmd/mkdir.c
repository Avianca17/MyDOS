#include <cmd/cmd.h>

void cmd_mkdir(char *args, int *cursor_y) {
    if (args[0] == '\0') {
        print("Usage: mkdir <dirname>", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    FRESULT result = make_dir(args);
    if (result == FR_EXIST) {
        print("mkdir: Directory already exists.", RED, 0, *cursor_y);
        scrl(cursor_y);
    } else if (result != FR_OK) {
        print("mkdir: Could not create directory.", RED, 0, *cursor_y);
        scrl(cursor_y);
    }
}
