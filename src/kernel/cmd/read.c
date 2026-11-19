#include <cmd/cmd.h>

void cmd_read(char *args, int *cursor_y) {
    if (args[0] == '\0') {
        print("Usage: read <filename>", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    FRESULT result = cat_file(args, cursor_y);
    if (result == FR_NO_FILE || result == FR_NO_PATH) {
        print("read: File not found.", RED, 0, *cursor_y);
        scrl(cursor_y);
    } else if (result != FR_OK) {
        print("read: Error reading file.", RED, 0, *cursor_y);
        scrl(cursor_y);
    }
}
