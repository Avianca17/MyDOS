#include <cmd/cmd.h>
#include <shell/env.h>
#include <shell/prompt.h>

void cmd_set(char *args, int *cursor_y) {
    while (*args == ' ') {
        args++;
    }

    if (*args == '\0') {
        print("PS1=", LIGHT_CYAN, 0, *cursor_y);
        print(shell_get_ps1(), WHITE, 4, *cursor_y);
        scrl(cursor_y);
        print("PATH=", LIGHT_CYAN, 0, *cursor_y);
        print(shell_env_get_path(), WHITE, 5, *cursor_y);
        scrl(cursor_y);
        return;
    }

    char *separator = strchr(args, '=');
    if (separator != 0) {
        *separator = '\0';
        char *value = separator + 1;

        if (strcmp(args, "PS1") == 0) {
            shell_set_ps1(value);
            print("set: PS1 updated", GREEN, 0, *cursor_y);
            scrl(cursor_y);
        } else if (strcmp(args, "PATH") == 0) {
            shell_env_set_path(value);
            print("set: PATH updated", GREEN, 0, *cursor_y);
            scrl(cursor_y);
        } else {
            print("set: unknown variable ", RED, 0, *cursor_y);
            print(args, WHITE, 21, *cursor_y);
            scrl(cursor_y);
        }
        return;
    }

    if (strcmp(args, "PS1") == 0) {
        print("PS1=", LIGHT_CYAN, 0, *cursor_y);
        print(shell_get_ps1(), WHITE, 4, *cursor_y);
        scrl(cursor_y);
    } else if (strcmp(args, "PATH") == 0) {
        print("PATH=", LIGHT_CYAN, 0, *cursor_y);
        print(shell_env_get_path(), WHITE, 5, *cursor_y);
        scrl(cursor_y);
    } else {
        print("set: unknown variable ", RED, 0, *cursor_y);
        print(args, WHITE, 21, *cursor_y);
        scrl(cursor_y);
    }
}
