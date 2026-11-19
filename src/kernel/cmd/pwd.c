#include <cmd/cmd.h>

void cmd_pwd(char *args, int *cursor_y) {
    char cwd[VGA_MAX_WIDTH];

    (void)args;

    FRESULT result = f_getcwd(cwd, sizeof(cwd));
    if (result == FR_OK) {
        print(cwd, WHITE, 0, *cursor_y);
    } else {
        print("pwd: Error retrieving current working directory.", RED, 0, *cursor_y);
    }
    scrl(cursor_y);
}
