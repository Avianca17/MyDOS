#include <cmd/cmd.h>

void cmd_mkfile(char *args, int *cursor_y) {
    if (args[0] == '\0') {
        print("Usage: mkfile <filename>", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    FRESULT result = make_file(args);

    if (result == FR_EXIST) {
        print("mkfile: File already exists.", RED, 0, *cursor_y);
        scrl(cursor_y);
    } else if (result != FR_OK) {
        print("mkfile: Could not create file.", RED, 0, *cursor_y);
        scrl(cursor_y);
    }
}
