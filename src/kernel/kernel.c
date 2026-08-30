#include "kernel.h"
#include "drivers/keyboard.h"

static unsigned int rtcts(unsigned char hour, unsigned char minute, unsigned char second) {
    return (hour * 3600u) + (minute * 60u) + second;
}

static void append_uint(char* out, unsigned int value) {
    char tmp[16];
    int len = 0;

    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0) {
        tmp[len++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (int i = 0; i < len; i++) {
        out[i] = tmp[len - 1 - i];
    }

    out[len] = '\0';
}

static void append_padded(char* out, unsigned int value, int width) {
    char tmp[16];
    int len = 0;

    if (value == 0) {
        tmp[len++] = '0';
    }
    else {
        while (value > 0) {
            tmp[len++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }

    while (len < width) {
        tmp[len++] = '0';
    }

    for (int i = 0; i < len; i++) {
        out[i] = tmp[len - 1 - i];
    }

    out[len] = '\0';
}

static unsigned int uptime = 0;

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

            cursor_y++;
            scroll_screen(&cursor_y);

            if (strcmp(cmd, "help") == 0) {
                print("Available commands:", WHITE, 0, cursor_y);

                cursor_y++;
                scroll_screen(&cursor_y);

                print("help clear echo sysinfo uptime shlogo date reboot shutdown", WHITE, 0, cursor_y);

                cursor_y++;
                scroll_screen(&cursor_y);
            }

            else if (strcmp(cmd, "clear") == 0) {
                clear_screen();
                cursor_y = 0;
            }

            else if (strncmp(cmd, "echo ", 5) == 0) {
                print(cmd + 5, WHITE, 0, cursor_y);

                cursor_y++;
                scroll_screen(&cursor_y);
            }

            else if (strcmp(cmd, "echo") == 0) {
                print("trml: ", RED, 0, cursor_y);
                print("echo requires text", WHITE, 6, cursor_y);

                cursor_y++;
                scroll_screen(&cursor_y);
            }

            else if (strcmp(cmd, "sysinfo") == 0) {
                char vendor[13];
                char ram_buf[32];

                unsigned int ram_mb = get_total_ram_mb(mbi);

                cpuid_vendor(vendor);
                itoanum(ram_mb, ram_buf);

                print("MyDOS Kernel: ", RED, 0, cursor_y);
                print("v0.1 beta", WHITE, 14, cursor_y);

                cursor_y++;
                scroll_screen(&cursor_y);

                print("Shell: ", GREEN, 0, cursor_y);
                print("trml v0.1 beta", WHITE, 7, cursor_y);

                cursor_y++;
                scroll_screen(&cursor_y);

                print("CPU Vendor: ", BLUE, 0, cursor_y);
                print(vendor, WHITE, 13, cursor_y);

                cursor_y++;
                scroll_screen(&cursor_y);

                print("RAM: ", RED, 0, cursor_y);
                print(ram_buf, WHITE, 6, cursor_y);
                print(" MB", WHITE, 10, cursor_y);

                cursor_y++;
                scroll_screen(&cursor_y);

                print("\n", WHITE, 0, cursor_y);

                cursor_y++;
                scroll_screen(&cursor_y);
            }
            else if (strcmp(cmd, "date") == 0) {
                char datetime[32];
                format_rtc_datetime(datetime);
                print(datetime, WHITE, 0, cursor_y);
                cursor_y++;
                scroll_screen(&cursor_y);
            }
            else if (strcmp(cmd, "uptime") == 0) {
                unsigned char sec = bcdtb(rtc_read(0x00));
                unsigned char min = bcdtb(rtc_read(0x02));
                unsigned char hr  = bcdtb(rtc_read(0x04));

                unsigned int now_seconds = rtcts(hr, min, sec);
                unsigned int elapsed = now_seconds - uptime;

                char uptime_str[32];
                int pos = 0;
                unsigned int d = elapsed / 86400u;
                unsigned int h = (elapsed % 86400u) / 3600u;
                unsigned int m = (elapsed % 3600u) / 60u;
                unsigned int s = elapsed % 60u;

                if (d > 0) {
                    char d_buf[16];
                    append_uint(d_buf, d);
                    for (int i = 0; d_buf[i] != '\0'; i++) {
                        uptime_str[pos++] = d_buf[i];
                    }
                    uptime_str[pos++] = 'd';
                    uptime_str[pos++] = ' ';
                }

                char h_buf[16];
                char m_buf[16];
                char s_buf[16];
                append_padded(h_buf, h, 2);
                append_padded(m_buf, m, 2);
                append_padded(s_buf, s, 2);

                uptime_str[pos++] = h_buf[0];
                uptime_str[pos++] = h_buf[1];
                uptime_str[pos++] = ':';
                uptime_str[pos++] = m_buf[0];
                uptime_str[pos++] = m_buf[1];
                uptime_str[pos++] = ':';
                uptime_str[pos++] = s_buf[0];
                uptime_str[pos++] = s_buf[1];
                uptime_str[pos] = '\0';

                print(uptime_str, WHITE, 0, cursor_y);
                cursor_y++;
                scroll_screen(&cursor_y);
            }
            else if (strcmp(cmd, "shlogo") == 0) {
                print_logo_at(0, cursor_y);
                cursor_y += 6;
                scroll_screen(&cursor_y);
            }

            else if (cmd[0] != '\0') {
                print("trml: unknown command", RED, 0, cursor_y);

                cursor_y++;
                scroll_screen(&cursor_y);
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
