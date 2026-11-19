#include <cmd/cmd.h>

void cmd_echo(char *args, int *cursor_y) {
    if (args[0] == '\0') {
        print("echo: echo requires some text", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    unsigned char color = WHITE;
    char *text = args;
    char *color_arg = 0;

    if (strncmp(args, "-c ", 3) == 0 || strncmp(args, "-c:", 3) == 0) {
        color_arg = args + 3;
    } else if (strncmp(args, "--color ", 8) == 0 || strncmp(args, "--color:", 8) == 0) {
        color_arg = args + 8;
    }

    if (color_arg != 0) {
        text = "";
        for (int i = 0; color_arg[i] != '\0'; i++) {
            if (color_arg[i] == ' ') {
                color_arg[i] = '\0';
                text = &color_arg[i + 1];
                break;
            }
        }
        color = parse_color(color_arg);
        if (color == 0xFF) {
            print("echo: unknown color, defaulting to default color. run 'help echo' for list",
                  RED, 0, *cursor_y);
            scrl(cursor_y);
            color = WHITE;
        }
    }

    if (text[0] == '\0') {
        print("trml: echo: echo requires text", RED, 0, *cursor_y);
        scrl(cursor_y);
    } else {
        print(text, color, 0, *cursor_y);
        scrl(cursor_y);
    }
}
