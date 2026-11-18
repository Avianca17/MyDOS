#include <cmd/cmd.h>

int doom_main(int argc, char **argv);

void cmd_doom(char *args, int *cursor_y) {
    char *argv[24];
    int argc = 0;

    argv[argc++] = "doom";
    char *token = cmd_next_token(&args);
    while (token != 0 && argc < 23) {
        argv[argc++] = token;
        token = cmd_next_token(&args);
    }
    argv[argc] = 0;

    int result = doom_main(argc, argv);

    clear_screen();
    if (result != 0) {
        print("doom: exited with an error", RED, 0, 0);
        *cursor_y = 1;
    } else {
        *cursor_y = 0;
    }
}
