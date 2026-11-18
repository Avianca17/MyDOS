#include <mydos.h>
#include <net/net.h>

#define KERNEL_VERSION "mk-0.5-stable"
#define SHELL_VERSION "trml-0.4"

static unsigned int uptime = 0;

static unsigned int elapsed_uptime_seconds(void) {
    unsigned char sec = bcdtb(rtc_read(0x00));
    unsigned char min = bcdtb(rtc_read(0x02));
    unsigned char hr = bcdtb(rtc_read(0x04));
    unsigned int now_seconds = rtcts(hr, min, sec);

    if (now_seconds < uptime) {
        now_seconds += 86400u;
    }

    return now_seconds - uptime;
}

static void format_short_uptime(unsigned int elapsed, char *out) {
    unsigned int days = elapsed / 86400u;
    unsigned int hours = (elapsed % 86400u) / 3600u;
    unsigned int minutes = (elapsed % 3600u) / 60u;
    unsigned int seconds = elapsed % 60u;
    int pos = 0;

    if (days > 0) {
        char days_buf[16];
        append_uint(days_buf, days);
        for (int i = 0; days_buf[i] != '\0'; i++) {
            out[pos++] = days_buf[i];
        }
        out[pos++] = 'd';
        out[pos++] = ' ';
    }

    out[pos++] = (char)('0' + hours / 10);
    out[pos++] = (char)('0' + hours % 10);
    out[pos++] = ':';
    out[pos++] = (char)('0' + minutes / 10);
    out[pos++] = (char)('0' + minutes % 10);
    out[pos++] = ':';
    out[pos++] = (char)('0' + seconds / 10);
    out[pos++] = (char)('0' + seconds % 10);
    out[pos] = '\0';
}

static char *next_token(char **input) {
    while (**input == ' ') {
        (*input)++;
    }
    if (**input == '\0') {
        return 0;
    }

    char *token = *input;
    while (**input != '\0' && **input != ' ') {
        (*input)++;
    }
    if (**input != '\0') {
        **input = '\0';
        (*input)++;
    }
    return token;
}

static int parse_drive_root(const char *path, BYTE *volume) {
    if (strlen(path) != 3 || path[0] < '0' || path[0] > '9' ||
        path[1] != ':' || path[2] != '/') {
        return 0;
    }

    BYTE drive = (BYTE)(path[0] - '0');
    if (drive >= FF_VOLUMES) {
        return 0;
    }

    *volume = drive;
    return 1;
}

static void mkfat(char *args, int *cursor_y) {
    char *arguments = args;
    char *format_flag = next_token(&arguments);
    char *filesystem = next_token(&arguments);
    char *path = next_token(&arguments);
    BYTE volume;
    int fat_version = 0;

    if (format_flag == 0 || filesystem == 0 || path == 0 ||
        next_token(&arguments) != 0 ||
        strcmp(format_flag, "-f") != 0 ||
        !parse_drive_root(path, &volume)) {
        print("Usage: mkfat -f <12|16|32> <drive>:/", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    if (strcmp(filesystem, "12") == 0) {
        fat_version = 12;
    } else if (strcmp(filesystem, "16") == 0) {
        fat_version = 16;
    } else if (strcmp(filesystem, "32") == 0) {
        fat_version = 32;
    } else {
        print("Usage: mkfat -f <12|16|32> <drive>:/", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    char drive_path[4] = {(char)('0' + volume), ':', '/', '\0'};
    print("Formatting drive ", YELLOW, 0, *cursor_y);
    print(drive_path, YELLOW, 18, *cursor_y);
    print(" as FAT", YELLOW, 20, *cursor_y);
    print(filesystem, YELLOW, 27, *cursor_y);
    print("; all data will be erased.", YELLOW, 29, *cursor_y);
    scrl(cursor_y);

    FRESULT result = fs_format_volume_type(volume, fat_version);
    if (result != FR_OK) {
        char code[32];
        itoanum((unsigned int)result, code);
        print("mkfat: formatting failed.", RED, 0, *cursor_y);
        print("FATFS error code: ", LIGHT_GREY, 0, *cursor_y);
        print(code, WHITE, 18, *cursor_y);
        scrl(cursor_y);
    } else {
        print("mkfat: FAT", GREEN, 0, *cursor_y);
        print(filesystem, GREEN, 10, *cursor_y);
        print(" format complete.", GREEN, 12, *cursor_y);
        scrl(cursor_y);
    }
}

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

    unsigned char sec = bcdtb(rtc_read(0x00));
    unsigned char min = bcdtb(rtc_read(0x02));
    unsigned char hr = bcdtb(rtc_read(0x04));

    uptime = rtcts(hr, min, sec);

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

            if (strcmp(cmd, "help") == 0) {
                cursor_y = shell_show_help(args, cursor_y);
            }

            else if (strcmp(cmd, "clear") == 0) {
                clear_screen();
                cursor_y = 0;
                if (strcmp(args, "-dl") == 0 || strcmp(args, "--draw-logo") == 0) {
                    print_logo();
                    cursor_y = 7;
                }
            } else if (strcmp(cmd, "change-theme") == 0) {
                if (args[0] == '\0' || change_theme(args) == 0) {
                    print("Please choose one of the built-in themes mentioned below:", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("default, matrix, ocean, sunset", LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
                    clear_screen();
                    print_logo();
                    cursor_y = 7;
                }
            } else if (strcmp(cmd, "loadkeys") == 0) {
                if (strcmp(args, "status") == 0) {
                    print("Active keyboard layout: ", WHITE, 0, cursor_y);
                    print(keyboard_get_layout_name(), GREEN, 24, cursor_y);
                    scrl(&cursor_y);
                } else if (args[0] == '\0') {
                    print("Usage: loadkeys <us|de|fr|status>", RED, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Available layouts: us, de, fr; use 'status' to check the active layout.", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                } else if (keyboard_load_layout(args)) {
                    print("Keyboard layout: ", WHITE, 0, cursor_y);
                    print(keyboard_get_layout_name(), GREEN, 19, cursor_y);
                    scrl(&cursor_y);
                } else {
                    print("loadkeys: unsupported layout. Try us, de, or fr.", RED, 0, cursor_y);
                    scrl(&cursor_y);
                }
            } else if (strcmp(cmd, "dhcp") == 0) {
                net_command(args, &cursor_y);
            } else if (strcmp(cmd, "echo") == 0) {
                if (args[0] == '\0') {
                    print("echo: echo requires some text", RED, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
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
                            print("echo: unknown color, defaulting to default color. run 'help echo' for list", RED, 0, cursor_y);
                            scrl(&cursor_y);
                            color = WHITE;
                        }
                    }

                    if (text[0] == '\0') {
                        print("trml: echo: echo requires text", RED, 0, cursor_y);
                        scrl(&cursor_y);
                    } else {
                        print(text, color, 0, cursor_y);
                        scrl(&cursor_y);
                    }
                }
            }

            else if (strcmp(cmd, "fetch") == 0) {
                char vendor[13];
                char ram_buf[32];
                char uptime_buf[32];

                unsigned int ram_mb = get_total_ram_mb(mbi);

                cpuid_vendor(vendor);
                itoanum(ram_mb, ram_buf);
                format_short_uptime(elapsed_uptime_seconds(), uptime_buf);

                while (cursor_y + 13 > vga_screen_height()) {
                    int scroll_y = vga_screen_height();
                    scroll_screen(&scroll_y);
                    cursor_y--;
                }

                print_logo_at(0, cursor_y);
                print("+--KEYS---------+--VALUES----------+", BLUE, 40, cursor_y);
                print("| User          | root             |", RED, 40, cursor_y + 1);
                print("| Host          | MyDOS            |", GREEN, 40, cursor_y + 2);
                print("| Kernel        | ", BLUE, 40, cursor_y + 3); 
                print(KERNEL_VERSION, WHITE, 58, cursor_y + 3);
                print("|", BLUE, 75, cursor_y + 3);
                print("| Shell         | ", YELLOW, 40, cursor_y + 4);
                print(SHELL_VERSION, WHITE, 58, cursor_y + 4);
                print("|", YELLOW, 75, cursor_y + 4);
                print("| Uptime        | ", LIGHT_MAGENTA, 40, cursor_y + 5);
                print(uptime_buf, WHITE, 58, cursor_y + 5);
                print("|", LIGHT_MAGENTA, 75, cursor_y + 5);
                print("| Processor     | ", MAGENTA, 40, cursor_y + 6);
                print(vendor, WHITE, 58, cursor_y + 6);
                print("|", MAGENTA, 75, cursor_y + 6);
                print("| Total RAM     | ", LIGHT_BLUE, 40, cursor_y + 7);
                print(ram_buf, WHITE, 58, cursor_y + 7);
                print(" MB", WHITE, 58 + strlen(ram_buf), cursor_y + 7);
                print("|", LIGHT_BLUE, 75, cursor_y + 7);
                print("+----------------+-----------------+", BLUE, 40, cursor_y + 8);

                static const char block[] = "**";

                print("+-COLORS---------+", BLUE, 40, cursor_y + 9);
                print("| ", BLUE, 40, cursor_y + 10);
                print(block, BLUE, 42, cursor_y + 10);
                print(block, GREEN, 44, cursor_y + 10);
                print(block, CYAN, 46, cursor_y + 10);
                print(block, RED, 48, cursor_y + 10);
                print(block, MAGENTA, 50, cursor_y + 10);
                print(block, BROWN, 52, cursor_y + 10);
                print(block, LIGHT_GREY, 54, cursor_y + 10);
                print("|", BLUE, 57, cursor_y + 10);

                print("| ", BLUE, 40, cursor_y + 11);
                print(block, DARK_GREY, 42, cursor_y + 11);
                print(block, LIGHT_BLUE, 44, cursor_y + 11);
                print(block, LIGHT_GREEN, 46, cursor_y + 11);
                print(block, LIGHT_CYAN, 48, cursor_y + 11);
                print(block, LIGHT_RED, 50, cursor_y + 11);
                print(block, LIGHT_MAGENTA, 52, cursor_y + 11);
                print(block, YELLOW, 54, cursor_y + 11);
                print("|", BLUE, 57, cursor_y + 11);
                print("+----------------+", BLUE, 40, cursor_y + 12);

                cursor_y += 12;
                scrl(&cursor_y);
            } else if (strcmp(cmd, "date") == 0) {
                char datetime[64];
                if (strcmp(args, "-t") == 0 || strcmp(args, "--time") == 0) {
                    format_rtc_time(datetime, 0);
                    print(datetime, WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                } else if (strcmp(args, "-d") == 0 || strcmp(args, "--date") == 0) {
                    format_rtc_date_only(datetime);
                    print(datetime, WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                } else if (strcmp(args, "-12") == 0 || strcmp(args, "--12h") == 0) {
                    format_rtc_time(datetime, 1);
                    print(datetime, WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                } else if (strcmp(args, "-h") == 0 || strcmp(args, "--help") == 0) {
                    print("Usage: date [-t|--time] [-d|--date] [-12|--12h]", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
                    format_rtc_datetime(datetime);
                    print(datetime, WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                }
            } else if (strcmp(cmd, "uptime") == 0) {
                unsigned int elapsed = elapsed_uptime_seconds();

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
                } else if (strcmp(args, "-s") == 0 || strcmp(args, "--seconds") == 0) {
                    char sec_buf[32];
                    append_uint(sec_buf, elapsed);
                    for (int i = 0; sec_buf[i] != '\0'; i++) {
                        uptime_str[pos++] = sec_buf[i];
                    }
                    uptime_str[pos++] = ' ';
                    uptime_str[pos++] = 's';
                    uptime_str[pos++] = 'e';
                    uptime_str[pos++] = 'c';
                    uptime_str[pos++] = 'o';
                    uptime_str[pos++] = 'n';
                    uptime_str[pos++] = 'd';
                    uptime_str[pos++] = 's';
                } else {
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
            } else if (strcmp(cmd, "beep") == 0) {
                pc_speaker_beep();
                print("BEEP! :)", GREEN, 0, cursor_y); scrl(&cursor_y);
            } else if (strcmp(cmd, "shlogo") == 0) {
                print_logo_at(0, cursor_y);
                cursor_y += 6;
                scrl(&cursor_y);
            } else if (strcmp(cmd, "mkfile") == 0) {
                if (args[0] == '\0') {
                    print("Usage: mkfile <filename>", RED, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
                    FRESULT result = make_file(args);

                    if (result == FR_OK) {
                        // print("mkfile: File created.", GREEN, 0, cursor_y);
                    } else if (result == FR_EXIST) {
                        print("mkfile: File already exists.", RED, 0, cursor_y); scrl(&cursor_y);
                    } else {
                        print("mkfile: Could not create file.", RED, 0, cursor_y); scrl(&cursor_y);
                    }

                    // scrl(&cursor_y);
                }
            } else if (strcmp(cmd, "cp") == 0) {
                cp_file(args, &cursor_y);
                scrl(&cursor_y);
            } else if (strcmp(cmd, "mv") == 0) {
                mv_file(args, &cursor_y);
                scrl(&cursor_y);
            } else if (strcmp(cmd, "rm") == 0) {
                FILINFO fno;
                if (args[0] == '\0') {
                    print("Usage: rm <filename>", RED, 0, cursor_y);
                } else if (strncmp(args, "-r", 2) == 0 || strncmp(args, "--recursive", 11) == 0) {
                    char *path = args;
                    while (*path && *path != ' ')
                        path++;
                    while (*path == ' ')
                        path++;

                    if (*path == '\0') {
                        print("rm: missing operand after '-r'", RED, 0, cursor_y);
                    } else {
                        FRESULT result = rm_recursive(path);
                        if (result == FR_OK) {
                            // print("rm: Directory removed.", GREEN, 0, cursor_y);
                        } else {
                            print("rm: Could not remove directory.", RED, 0, cursor_y);
                        }
                    }
                } else if (f_stat(args, &fno) != FR_OK) {
                    print("rm: File not found.", RED, 0, cursor_y);
                } else if (fno.fattrib & AM_DIR) {
                    print("rm: Cannot remove directory. Use the -r flag.", RED, 0, cursor_y);
                } else {
                    FRESULT result = rm_file(args);
                    if (result == FR_OK) {
                        // print("rm: File removed.", GREEN, 0, cursor_y);
                    } else {
                        print("rm: Could not remove file.", RED, 0, cursor_y); scrl(&cursor_y);
                    }
                }
            } else if (strcmp(cmd, "mkdir") == 0) {
                if (args[0] == '\0') {
                    print("Usage: mkdir <dirname>", RED, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
                    FRESULT result = make_dir(args);
                    if (result == FR_OK) {
                        // print("mkdir: Directory created.", GREEN, 0, cursor_y);
                    } else if (result == FR_EXIST) {
                        print("mkdir: Directory already exists.", RED, 0, cursor_y); scrl(&cursor_y);
                    } else {
                        print("mkdir: Could not create directory.", RED, 0, cursor_y); scrl(&cursor_y);
                    }
                }
            } else if (strcmp(cmd, "ls") == 0) {
                const char *list_path = (args[0] != '\0') ? args : ".";
                FRESULT result = list_files(list_path, &cursor_y);

                if (result != FR_OK) {
                    print("ls: Could not list files.", RED, 0, cursor_y);
                    scrl(&cursor_y);
                }
            } else if (strcmp(cmd, "lsdrv") == 0) {
                if (args[0] != '\0') { print("Usage: lsdrv", RED, 0, cursor_y);} 
                else { list_drives(&cursor_y); }
            } else if (strcmp(cmd, "mkfat") == 0) { mkfat(args, &cursor_y); } 
            else if (strcmp(cmd, "cd") == 0) {
                if (args[0] == '\0') {
                    print("Usage: cd /PATH/TO/DIRECTORY", RED, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
                    FRESULT result = f_chdir(args);
                    if (result == FR_NO_PATH) {
                        print("cd: Directory not found", RED, 0, cursor_y);
                        scrl(&cursor_y);
                    }
                }
            } else if (strcmp(cmd, "read") == 0) {
                if (args[0] == '\0') {
                    print("Usage: read <filename>", RED, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
                    FRESULT res = cat_file(args, &cursor_y);
                    if (res == FR_NO_FILE || res == FR_NO_PATH) {
                        print("read: File not found.", RED, 0, cursor_y);
                        scrl(&cursor_y);
                    } else if (res != FR_OK) {
                        print("read: Error reading file.", RED, 0, cursor_y);
                        scrl(&cursor_y);
                    }
                }
            } else if (strcmp(cmd, "myeditor") == 0) {
                myeditor();
                cursor_y = 0;
            } else if (strcmp(cmd, "calc") == 0) {
                main_calc();
                cursor_y = 0;
            } else if (strcmp(cmd, "pwd") == 0) {
                FRESULT result = f_getcwd(cmd, sizeof(cmd));
                if (result == FR_OK) {
                    print(cmd, WHITE, 0, cursor_y);
                } else {
                    print("pwd: Error retrieving current working directory.", RED, 0, cursor_y);
                }
                scrl(&cursor_y);
                // Fun little easter egg here! Only implemented it cause I develop MyDOS on Gentoo, and I really love it sooooooooo 
            } else if (strcmp(cmd, "larry") == 0) {
                typedef struct {
                    const char *lines[3];
                    unsigned int line_count;
                } larry_message_t;

                static const larry_message_t larry_messages[] = {
                    {{" < Live, Love, Compile! >", 0, 0}, 1},
                    {{" < Have you mooed today? >", 0, 0}, 1},
                    {{" < Hello, world! >", 0, 0}, 1},
                    {{" < Happy coding, everyone! >", 0, 0}, 1},
                    {{
                        " < Uh, I see his balls, Juicy >",
                        " <      - Gigsteak15          >"
                    }, 2},
                    {{" < Gentoo is when opsec btw >", 0, 0}, 1},
                    {{" < doas emerge --ask kernel/mydos >", 0, 0}, 1},
                    {{" < tree  - v1sta_ >", 0, 0}, 1},
                    {{" < gentoo is a gentoo  - gentoo cunt >", 0, 0}, 1},
                    {{
                        " <     New to MyDOS? Don't be creeped out by the lack     >",
                        " <       of GUI! It's a very simple OS that even a        >",
                        " < child could use! Type 'help' if you ever get confused. >"
                    }, 3},
                    {{
                        " < There is a pipe bomb at your door >",
                        " <           - Pitbull               >"}
                    , 2},
                    {{
                         " < Message from the owner of MyDOS: >",
                         " < Gentoo is the best Linux distro  >",
                         " < and you can't change my mind!    >"
                    }, 3},
                    {{
                        " < larp is when netbsd >",
                        " <     - yaffs2        >"
                    }, 2},
                    {{" < the - Boingboing737 >", 0, 0}, 1},
                    {{
                        " < INSTALL VISNUX >",
                        " <    - v1sta_     >"
                    }, 2},
                    {{" < Happy birthday, pluto! >", 0, 0}, 1},
                    {{" < Install Gentoo Linux on your PC today! >", 0, 0}, 1},
                    {{" < I wish I could get a free bag of chips... >", 0, 0}, 1},
                    {{
                        " < I only eat compost >",
                        " <    - Poko_Banana   >"
                    }, 2},
                    {{
                        " < Thank you blyat suka suka suka thank >",
                        " <      you for subscribe and like      >",
                        " <           - danielka552              >"
                    }, 3},
                    {{
                        " < its for my kernel >",
                        " <    - mayainit     >"
                    }, 2},
                };
                static unsigned int larry_message_index;
                const larry_message_t *message = &larry_messages[larry_message_index];
                int border_width = 0;

                for (unsigned int i = 0; i < message->line_count; i++) {
                    int line_width = strlen(message->lines[i]);
                    if (line_width > border_width) {
                        border_width = line_width;
                    }
                }
                border_width += 2;
                if (border_width > vga_screen_width()) {
                    border_width = vga_screen_width();
                }
                border_width -= 3;
                char border[VGA_MAX_WIDTH + 1];
                border[0] = ' ';
                border[1] = ' ';
                for (int i = 2; i < border_width; i++) {
                    border[i] = '_';
                }
                border[border_width] = '\0';

                print("Larry loves Gentoo (Linux)", WHITE, 0, cursor_y);
                scrl(&cursor_y); scrl(&cursor_y);
                print(border, LIGHT_GREY, 0, cursor_y); scrl(&cursor_y);
                for (unsigned int i = 0; i < message->line_count; i++) {
                    print(message->lines[i], LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                }
                for (int i = 2; i < border_width; i++) {
                    border[i] = '-';
                }
                print(border, LIGHT_GREY, 0, cursor_y); scrl(&cursor_y);
                print("        \\   ^__^", LIGHT_GREY, 0, cursor_y); scrl(&cursor_y);
                print("         \\  (oo)\\_______", LIGHT_GREY, 0, cursor_y); scrl(&cursor_y);
                print("            (__)\\        )\\/\\", LIGHT_GREY, 0, cursor_y); scrl(&cursor_y);
                print("                ||----w |", LIGHT_GREY, 0, cursor_y); scrl(&cursor_y);
                print("                ||     ||", LIGHT_GREY, 0, cursor_y); scrl(&cursor_y);
                scrl(&cursor_y);
                larry_message_index =
                    (larry_message_index + 1) %
                    (sizeof(larry_messages) / sizeof(larry_messages[0]));
            }
            else if (strcmp(cmd, "mk-mbr-part") == 0) {
                char *arguments = args;
                char *path = next_token(&arguments);
                BYTE volume;

                if (path == 0 ||
                    next_token(&arguments) != 0 ||
                    !parse_drive_root(path, &volume)) {
                    print("Usage: mk-mbr-part <drive>:/", RED, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
                    LBA_t plist[] = {100, 0};
                    BYTE work[FF_MAX_SS];

                    FRESULT result = f_fdisk(volume, plist, work);
                    switch (result) {
                        case FR_OK: {
                            #if FF_MULTI_PARTITION
                            VolToPart[volume].pd = volume;
                            VolToPart[volume].pt = 1;
                            #endif
                            TCHAR volume_path[3] = {(TCHAR)('0' + volume), ':', '\0'};
                            MKFS_PARM options = {
                                .fmt = FM_FAT32,
                                .n_fat = 1,
                                .align = 0,
                                .n_root = 0,
                                .au_size = 0
                            };
                            result = f_mkfs(volume_path, &options, work, sizeof work);
                            switch (result) {
                                case FR_OK:
                                    print("mk-mbr-part: MBR and FAT32 setup complete.",
                                          GREEN, 0, cursor_y);
                                    scrl(&cursor_y);
                                    break;
                                case FR_DISK_ERR:
                                    print("mk-mbr-part: Disk error during format!", RED, 0, cursor_y);
                                    scrl(&cursor_y);
                                    break;
                                case FR_NOT_READY:
                                    print("mk-mbr-part: Disk is not ready!", RED, 0, cursor_y);
                                    scrl(&cursor_y);
                                    break;
                                case FR_WRITE_PROTECTED:
                                    print("mk-mbr-part: Disk is write-protected!", RED, 0, cursor_y);
                                    scrl(&cursor_y);
                                    break;
                                case FR_INVALID_PARAMETER:
                                    print("mk-mbr-part: Invalid FAT32 format parameter!", RED, 0, cursor_y);
                                    scrl(&cursor_y);
                                    break;
                                case FR_MKFS_ABORTED:
                                    print("mk-mbr-part: FAT32 format aborted!", RED, 0, cursor_y);
                                    scrl(&cursor_y);
                                    break;
                                case FR_NOT_ENOUGH_CORE:
                                    print("mk-mbr-part: Not enough memory!", RED, 0, cursor_y);
                                    scrl(&cursor_y);
                                    break;
                                default:
                                    print("mk-mbr-part: FAT32 format failed!", RED, 0, cursor_y);
                                    scrl(&cursor_y);
                                    break;
                            }
                            break;
                        }
                        case FR_DISK_ERR:
                            print("mk-mbr-part: Disk error while creating MBR!", RED, 0, cursor_y);
                            scrl(&cursor_y);
                            break;
                        case FR_NOT_READY:
                            print("mk-mbr-part: Disk is not ready!", RED, 0, cursor_y);
                            scrl(&cursor_y);
                            break;
                        case FR_WRITE_PROTECTED:
                            print("mk-mbr-part: Disk is write-protected!", RED, 0, cursor_y);
                            scrl(&cursor_y);
                            break;
                        case FR_INVALID_PARAMETER:
                            print("mk-mbr-part: Invalid partition parameter!", RED, 0, cursor_y);
                            scrl(&cursor_y);
                            break;
                        case FR_NOT_ENOUGH_CORE:
                            print("mk-mbr-part: Not enough memory!", RED, 0, cursor_y);
                            scrl(&cursor_y);
                            break;
                        default:
                            print("mk-mbr-part: MBR partitioning failed!", RED, 0, cursor_y);
                            scrl(&cursor_y);
                            break;
                    }
                }

            }

            else if (strcmp(cmd, "shtdwn") == 0) {
                shutdown();
            } else if (strcmp(cmd, "kp") == 0) {
                __asm__ volatile("xorl %%eax, %%eax\n\t"
                                 "xorl %%edx, %%edx\n\t"
                                 "xorl %%ecx, %%ecx\n\t"
                                 "divl %%ecx"
                                 :
                                 :
                                 : "eax", "ecx", "edx", "cc");
            } else if (strcmp(cmd, "rbt") == 0) {
                __asm__ volatile("movb $0xFE, %%al\n\t"
                                 "outb %%al, $0x64\n\t"
                                 :
                                 :
                                 : "al");
            } else if (cmd[0] != '\0') {
                print("trml: unknown command, try again or run 'help' for assistance", RED, 0, cursor_y);
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
