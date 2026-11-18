#include <cmd/cmd.h>

void cmd_cd(char *args, int *cursor_y) {
    if (args[0] == '\0') {
        print("Usage: cd /PATH/TO/DIRECTORY", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    FRESULT result = f_chdir(args);
    if (result == FR_NO_PATH) {
        print("cd: Directory not found", RED, 0, *cursor_y);
        scrl(cursor_y);
    }
}
