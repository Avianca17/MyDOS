#include <mydos.h>
#include <cmd/cmd.h>
#include <shell/prompt.h>

void shell_run(unsigned int mbi, int cursor_y) {
    int cursor_x = 0;
    int prompt_width = shell_prompt_render(cursor_y, &cursor_x);
    int max_cmd_len = vga_screen_width() - prompt_width - 1;
    if (max_cmd_len > VGA_MAX_WIDTH)
        max_cmd_len = VGA_MAX_WIDTH;
    if (max_cmd_len < 0)
        max_cmd_len = 0;
    move_cursor(cursor_x, cursor_y);

    char cmd[VGA_MAX_WIDTH + 1];
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
            if (cursor_x > prompt_width && cmd_len > 0) {
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
                    cmd[i] = '\0';
                    args = &cmd[i + 1];
                    break;
                }
            }

            scrl(&cursor_y);

            if (cmd[0] != '\0' && !cmd_run(cmd, args, &cursor_y)) {
                print("shell: unknown command, try again or run 'help' for assistance",
                      RED, 0, cursor_y);
                scrl(&cursor_y);
            }

            cursor_x = 0;
            prompt_width = shell_prompt_render(cursor_y, &cursor_x);
            max_cmd_len = vga_screen_width() - prompt_width - 1;
            if (max_cmd_len > VGA_MAX_WIDTH)
                max_cmd_len = VGA_MAX_WIDTH;
            if (max_cmd_len < 0)
                max_cmd_len = 0;

            cmd_len = 0;
            move_cursor(cursor_x, cursor_y);
        }

        else {
            if (cmd_len < max_cmd_len) {
                cmd[cmd_len++] = c;

                char str[2] = {c, '\0'};
                print(str, WHITE, cursor_x, cursor_y);

                cursor_x++;
                move_cursor(cursor_x, cursor_y);
            }
        }
    }
}
