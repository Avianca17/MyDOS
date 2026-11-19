#include <cmd/cmd.h>

void cmd_ls(char *args, int *cursor_y) {
    const char *list_path = (args[0] != '\0') ? args : ".";
    FRESULT result = list_files(list_path, cursor_y);

    if (result != FR_OK) {
        print("ls: Could not list files.", RED, 0, *cursor_y);
        scrl(cursor_y);
    }
}
