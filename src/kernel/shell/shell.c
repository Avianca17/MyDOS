#include <mydos.h>
#include <cmd/cmd.h>

void shell_run(unsigned int mbi, int cursor_y) {
    int cursor_x = PROMPT_LENGTH;

    print("MyDOS", GREEN, 0, cursor_y);
    print(":", WHITE, 5, cursor_y);
    print("[", RED, 6, cursor_y);
    print("root", LIGHT_RED, 7, cursor_y);
    print("]", RED, 11, cursor_y);
    print("#", WHITE, 12, cursor_y);
    move_cursor(cursor_x, cursor_y);
    // MyDOS:[root]# _

    char cmd[VGA_MAX_WIDTH - PROMPT_LENGTH + 1];
    int cmd_len = 0;

    cmd_set_mbi(mbi);
    cmd_uptime_init();

    print("Thank you for trying out MyDOS!", WHITE, 0, 7);
    print("If you found a bug or have a suggestion, message me on Discord! aviancaflight17", WHITE, 0, 8);

    while (1) {
        char c = keyboard_getchar();

        if (c == 0)
            continue;

        if (c == '\b') {
            if (cursor_x > PROMPT_LENGTH && cmd_len > 0) {
                cmd_len--;
                cmd[cmd_len] = '\0';

                cursor_x--;
                print(" ", WHITE, cursor_x, cursor_y);
                move_cursor(cursor_x, cursor_y);
            }
        }

        else if (c == '\n') {
            cmd[cmd_len] = '\0';

            char *args = "";
            for (int i = 0; cmd[i] != '\0'; i++) {
                if (cmd[i] == ' ') {
                    cmd[i] = '\0';      // Split the command
                    args = &cmd[i + 1]; // Everything after the space is considered arguments
                    break;
                }
            }

            scrl(&cursor_y);

            if (cmd[0] != '\0' && !cmd_run(cmd, args, &cursor_y)) {
                print("trml: unknown command, try again or run 'help' for assistance",
                      RED, 0, cursor_y);
                scrl(&cursor_y);
            }

            print("MyDOS", GREEN, 0, cursor_y);
            print(":", WHITE, 5, cursor_y);
            print("[", RED, 6, cursor_y);
            print("root", LIGHT_RED, 7, cursor_y);
            print("]", RED, 11, cursor_y);
            print("#", WHITE, 12, cursor_y);
            move_cursor(cursor_x, cursor_y);

            cursor_x = PROMPT_LENGTH;
            cmd_len = 0;

            move_cursor(cursor_x, cursor_y);
        }

        else {
            if (cmd_len < vga_screen_width() - PROMPT_LENGTH) {
                cmd[cmd_len++] = c;

                char str[2] = {c, '\0'};

                print(str, WHITE, cursor_x, cursor_y);

                cursor_x++;
                move_cursor(cursor_x, cursor_y);
            }
        }
    }
}
