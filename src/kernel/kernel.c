#include "kernel.h"
#include "drivers/keyboard.h"

static unsigned int uptime = 0;

void scrl(int* cursor_y);

void kernel_main(unsigned int magic, unsigned int mbi) {
    (void)magic;

    clear_screen();
    print_logo();

    int cursor_y = 7;
    int cursor_x = PROMPT_LENGTH;

    print("user@MyDOS~> ", GREEN, 0, cursor_y);
    move_cursor(cursor_x, cursor_y);

    char cmd[64];
    int cmd_len = 0;

    unsigned char sec = bcdtb(rtc_read(0x00));
    unsigned char min = bcdtb(rtc_read(0x02));
    unsigned char hr = bcdtb(rtc_read(0x04));

    uptime = rtcts(hr, min, sec);

    while (1) {
        char c = keyboard_getchar();

        if (c == 0) continue;

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

            char* args = "";
            for (int i=0; cmd[i] != '\0'; i++) {
                if (cmd[i] == ' ') {
                    cmd[i] = '\0'; // Split the command
                    args = &cmd[i + 1]; // Everything after the space is considered arguments
                    break;
                }
            }

            scrl(&cursor_y);

            if (strcmp(cmd, "help") == 0) {
                print("Available commands:", WHITE, 0, cursor_y); scrl(&cursor_y);
                print("help clear echo sysinfo uptime shlogo date rbt shtdwn", WHITE, 0, cursor_y); scrl(&cursor_y);
            }

            else if (strcmp(cmd, "clear") == 0) { clear_screen(); cursor_y = 0; }
            else if (strcmp(cmd, "echo") == 0) {
                if (args[0] == '\0') { print("trml: echo requires text", RED, 0, cursor_y); scrl(&cursor_y); }
                else {
                    unsigned char color = WHITE; // Default color
                    char* text = args;
                    char* color_arg = 0;
                    
                    if (strncmp(args, "-c ", 3) == 0 || strncmp(args, "-c:", 3) == 0) { color_arg = args + 3; } // Check for 3 character prefix
                    else if (strncmp(args, "--color ", 8) == 0 || strncmp(args, "--color:", 8) == 0) { color_arg = args + 8; } // Check for 8 character prefix
                    if (color_arg != 0) { // If a flag is passed isolate color and twxt
                        text = "";
                        for (int i = 0; color_arg[i] != '\0'; i++) {
                            if (color_arg[i] == ' ') {
                                color_arg[i] = '\0';
                                text = &color_arg[i + 1];
                                break;
                            }
                        }
                        color = parse_color(color_arg);
                    }

                    // Check if theres any text to print
                    if (text[0] == '\0') { print("trml: echo requires text", RED, 0, cursor_y); scrl(&cursor_y); }
                    else { print(text, color, 0, cursor_y); scrl(&cursor_y); }
                    // print(text, WHITE, 0, cursor_y); scrl(&cursor_y); 
                }
            }

            else if (strcmp(cmd, "sysinfo") == 0) {
                char vendor[13];
                char ram_buf[32];

                unsigned int ram_mb = get_total_ram_mb(mbi);

                cpuid_vendor(vendor);
                itoanum(ram_mb, ram_buf);

                print("MyDOS Kernel: ", RED, 0, cursor_y);
                print("v0.1 beta", WHITE, 14, cursor_y);

                scrl(&cursor_y);

                print("Shell: ", GREEN, 0, cursor_y);
                print("trml v0.1 beta", WHITE, 7, cursor_y);

                scrl(&cursor_y);

                print("CPU Vendor: ", BLUE, 0, cursor_y);
                print(vendor, WHITE, 13, cursor_y);

                scrl(&cursor_y);

                print("RAM: ", RED, 0, cursor_y);
                print(ram_buf, WHITE, 6, cursor_y);
                print(" MB", WHITE, 10, cursor_y);

                scrl(&cursor_y);

                print("\n", WHITE, 0, cursor_y);

                scrl(&cursor_y);
            }
            else if (strcmp(cmd, "date") == 0) {
                char datetime[32];
                format_rtc_datetime(datetime);
                print(datetime, WHITE, 0, cursor_y);
                scrl(&cursor_y);
            }
            else if (strcmp(cmd, "uptime") == 0) {
                unsigned char sec = bcdtb(rtc_read(0x00));
                unsigned char min = bcdtb(rtc_read(0x02));
                unsigned char hr  = bcdtb(rtc_read(0x04));

                unsigned int now_seconds = rtcts(hr, min, sec);
                if (now_seconds < uptime) {
                    now_seconds += 86400u;
                }
                unsigned int elapsed = now_seconds - uptime;

                char uptime_str[64];
                int pos = 0;
                unsigned int d = elapsed / 86400u;
                unsigned int h = (elapsed % 86400u) / 3600u;
                unsigned int m = (elapsed % 3600u) / 60u;
                unsigned int s = elapsed % 60u;

                int pretty = (strcmp(args, "-p") == 0) || (strcmp(args, "--pretty") == 0);

                if (pretty) {
                    char value_buf[16];

                    uptime_str[pos++] = 'u';
                    uptime_str[pos++] = 'p';
                    uptime_str[pos++] = ' ';
                    uptime_str[pos++] = 'f';
                    uptime_str[pos++] = 'o';
                    uptime_str[pos++] = 'r';
                    uptime_str[pos++] = ' ';

                    append_uint(value_buf, d);
                    for (int i = 0; value_buf[i] != '\0'; i++) {
                        uptime_str[pos++] = value_buf[i];
                    }
                    uptime_str[pos++] = ' ';
                    uptime_str[pos++] = 'd';
                    uptime_str[pos++] = 'a';
                    uptime_str[pos++] = 'y';
                    uptime_str[pos++] = 's';
                    uptime_str[pos++] = ' ';

                    append_uint(value_buf, h);
                    for (int i = 0; value_buf[i] != '\0'; i++) {
                        uptime_str[pos++] = value_buf[i];
                    }
                    uptime_str[pos++] = ' ';
                    uptime_str[pos++] = 'h';
                    uptime_str[pos++] = 'o';
                    uptime_str[pos++] = 'u';
                    uptime_str[pos++] = 'r';
                    uptime_str[pos++] = 's';
                    uptime_str[pos++] = ' ';

                    append_uint(value_buf, m);
                    for (int i = 0; value_buf[i] != '\0'; i++) {
                        uptime_str[pos++] = value_buf[i];
                    }
                    uptime_str[pos++] = ' ';
                    uptime_str[pos++] = 'm';
                    uptime_str[pos++] = 'i';
                    uptime_str[pos++] = 'n';
                    uptime_str[pos++] = 'u';
                    uptime_str[pos++] = 't';
                    uptime_str[pos++] = 'e';
                    uptime_str[pos++] = 's';
                    uptime_str[pos++] = ' ';
                    uptime_str[pos++] = 'a';
                    uptime_str[pos++] = 'n';
                    uptime_str[pos++] = 'd';
                    uptime_str[pos++] = ' ';

                    append_uint(value_buf, s);
                    for (int i = 0; value_buf[i] != '\0'; i++) {
                        uptime_str[pos++] = value_buf[i];
                    }
                    uptime_str[pos++] = ' ';
                    uptime_str[pos++] = 's';
                    uptime_str[pos++] = 'e';
                    uptime_str[pos++] = 'c';
                    uptime_str[pos++] = 'o';
                    uptime_str[pos++] = 'n';
                    uptime_str[pos++] = 'd';
                    uptime_str[pos++] = 's';
                }
                else {
                    if (d > 0) {
                        char d_buf[16];
                        append_uint(d_buf, d);
                        for (int i = 0; d_buf[i] != '\0'; i++) {
                            uptime_str[pos++] = d_buf[i];
                        }
                        uptime_str[pos++] = 'd';
                        uptime_str[pos++] = ' ';
                    }

                    uptime_str[pos++] = h / 10 + '0';
                    uptime_str[pos++] = h % 10 + '0';
                    uptime_str[pos++] = ':';
                    uptime_str[pos++] = m / 10 + '0';
                    uptime_str[pos++] = m % 10 + '0';
                    uptime_str[pos++] = ':';
                    uptime_str[pos++] = s / 10 + '0';
                    uptime_str[pos++] = s % 10 + '0';
                }
                uptime_str[pos] = '\0';
                print(uptime_str, WHITE, 0, cursor_y);
                scrl(&cursor_y);
            }
            else if (strcmp(cmd, "shlogo") == 0) {
                print_logo_at(0, cursor_y);
                cursor_y += 6;
                scrl(&cursor_y);
            }
            else if (strcmp(cmd, "shtdwn") == 0) {
                shutdown();
            }
            else if (strcmp(cmd, "rbt") == 0) {
                __asm__ volatile(
                    "movb $0xFE, %%al\n\t"
                    "outb %%al, $0x64\n\t"
                    :
                    :
                    : "al"
                );
            }
            else if (cmd[0] != '\0') {
                print("trml: unknown command", RED, 0, cursor_y);

                scrl(&cursor_y);
            }

            print("user@MyDOS~> ", GREEN, 0, cursor_y);

            cursor_x = PROMPT_LENGTH;
            cmd_len = 0;

            move_cursor(cursor_x, cursor_y);
        }

        else {
            if (cmd_len < 63) {
                cmd[cmd_len++] = c;

                char str[2] = { c, '\0' };

                print(str, WHITE, cursor_x, cursor_y);

                cursor_x++;
                move_cursor(cursor_x, cursor_y);
            }
        }
    }
}

void scrl(int* cursor_y){
    (*cursor_y)++;
    scroll_screen(cursor_y);
}