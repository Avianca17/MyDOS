#include <cmd/cmd.h>

void cmd_rm(char *args, int *cursor_y) {
    FILINFO fno;

    if (args[0] == '\0') {
        print("Usage: rm <filename>", RED, 0, *cursor_y);
        return;
    }

    if (strncmp(args, "-r", 2) == 0 || strncmp(args, "--recursive", 11) == 0) {
        char *path = args;
        while (*path && *path != ' ') {
            path++;
        }
        while (*path == ' ') {
            path++;
        }

        if (*path == '\0') {
            print("rm: missing operand after '-r'", RED, 0, *cursor_y);
        } else if (rm_recursive(path) != FR_OK) {
            print("rm: Could not remove directory.", RED, 0, *cursor_y);
        }
    } else if (f_stat(args, &fno) != FR_OK) {
        print("rm: File not found.", RED, 0, *cursor_y);
    } else if (fno.fattrib & AM_DIR) {
        print("rm: Cannot remove directory. Use the -r flag.", RED, 0, *cursor_y);
    } else if (rm_file(args) != FR_OK) {
        print("rm: Could not remove file.", RED, 0, *cursor_y);
        scrl(cursor_y);
    }
}
