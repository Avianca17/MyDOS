#include "kernel.h"
#include "drivers/keyboard.h"

#define KERNEL_VERSION "v0.3.1-ex"
#define SHELL_VERSION "v0.3-stable"

static unsigned int uptime = 0;

void scrl(int *cursor_y);

static const char *format_error_message(FRESULT result) {
    switch (result) {
        case FR_DISK_ERR:
            return "FAT32 format failed: disk I/O error.";
        case FR_NOT_READY:
            return "FAT32 format failed: disk not ready.";
        case FR_WRITE_PROTECTED:
            return "FAT32 format failed: disk is write-protected.";
        case FR_MKFS_ABORTED:
            return "FAT32 format failed: disk could not be formatted.";
        default:
            return "FAT32 format failed: filesystem error.";
    }
}

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

static void help_row(const char *text, unsigned char color, int *cursor_y) {
    print(text, color, 2, *cursor_y);
    scrl(cursor_y);
}

static void help_command_header(const char *command, int *cursor_y) {
    help_row("============================================================", BLUE, cursor_y);
    help_row("                   MYDOS / COMMAND MANUAL", GREEN, cursor_y);
    help_row("------------------------------------------------------------", BLUE, cursor_y);
    print("  COMMAND  ", LIGHT_CYAN, 2, *cursor_y);
    print(command, YELLOW, 13, *cursor_y);
    scrl(cursor_y);
    help_row("------------------------------------------------------------", BLUE, cursor_y);
}

void KernelEntryPoint(unsigned int magic, unsigned int mbi) {
    (void)magic;
    (void)mbi;

    // Direct boot-time test for kernel panic (remove or comment out when done testing):
    // panic("Fatal exception: system integrity verification failed");

    gdt_init();
    idt_init();
    keyboard_load_layout("de");

    clear_screen();
    print_logo();

    int cursor_y = 13;

    FRESULT fs_result = fs_init();

    if (fs_result == FR_NO_FILESYSTEM) {
        print("No recognized filesystem. Format this disk to FAT32? [y/N]", YELLOW, 0, cursor_y);
        scrl(&cursor_y);

        char answer;
        do {
            answer = keyboard_getchar();
        } while (answer != 'y' && answer != 'Y' && answer != 'n' && answer != 'N');

        if (answer == 'y' || answer == 'Y') {
            fs_result = fs_format();
            if (fs_result != FR_OK) {
                print(format_error_message(fs_result), RED, 0, cursor_y);
                scrl(&cursor_y);
            }
        } else {
            print("Disk left unchanged.", WHITE, 0, cursor_y);
            scrl(&cursor_y);
        }
    } else if (fs_result == FR_NOT_READY) {
        print("No usable disk detected.", RED, 0, cursor_y);
        scrl(&cursor_y);
    } else if (fs_result != FR_OK) {
        print("MyDOS couldn't access the disk.", RED, 0, cursor_y);
        scrl(&cursor_y);
    }
    int cursor_x = PROMPT_LENGTH;

    print("MyDOS", GREEN, 0, cursor_y);
    print("[", RED, 5, cursor_y);
    print("root", LIGHT_RED, 6, cursor_y);
    print("]", RED, 10, cursor_y);
    print("/", LIGHT_RED, 11, cursor_y);
    print(":~#", WHITE, 12, cursor_y);
    move_cursor(cursor_x, cursor_y);
    // MyDOS[root]/:~# _

    char cmd[64];
    int cmd_len = 0;
    int a = 12;

    unsigned char sec = bcdtb(rtc_read(0x00));
    unsigned char min = bcdtb(rtc_read(0x02));
    unsigned char hr = bcdtb(rtc_read(0x04));

    uptime = rtcts(hr, min, sec);

    print("Welcome and thank you for trying out MyDOS!", WHITE, 0, 7);
    print("Keep in mind that it is still in beta, so there will not be many features", WHITE, 0, 8);
    print("and there might be bugs. If you can, check out my GitHub repository at: ", WHITE, 0, 9);
    print("---https://github.com/Avianca17/MyDOS---", GREEN, 0, 10);
    print("That's all from me! Type 'help' to see available commands.", WHITE, 0, 11);
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
                if (args[0] != '\0') {
                    help_command_header(args, &cursor_y);
                }

                if (args[0] == '\0') {
                    help_row("+--------------------------------------------------------------------------+", BLUE, &cursor_y);
                    help_row("|                           MYDOS COMMAND CENTRE                           |", GREEN, &cursor_y);
                    help_row("+--------------------------------------------------------------------------+", BLUE, &cursor_y);
                    help_row("| Want to know what a command does? Just type help <cmd>!                  |", WHITE, &cursor_y);
                    help_row("|                                                                          |", WHITE, &cursor_y);
                    help_row("| [ SHELL ]                                                                |", YELLOW, &cursor_y);
                    help_row("|   help            clear           echo           change-theme            |", LIGHT_GREY, &cursor_y);
                    help_row("|   loadkeys        date           uptime          fetch                   |", LIGHT_GREY, &cursor_y);
                    help_row("|   shlogo          beep                                                   |", LIGHT_GREY, &cursor_y);
                    help_row("|                                                                          |", WHITE, &cursor_y);
                    help_row("| [ SYSTEM AND DEBUGGING ]                                                 |", LIGHT_CYAN, &cursor_y);
                    help_row("|   rbt             shtdwn           fault                                 |", LIGHT_GREY, &cursor_y);
                    help_row("|                                                                          |", WHITE, &cursor_y);
                    help_row("| [ FILESYSTEM ]                                                           |", LIGHT_GREEN, &cursor_y);
                    help_row("|   mkdir           mkfile          ls             lsdrv                   |", LIGHT_GREY, &cursor_y);
                    help_row("|   rm              cd              myeditor       pwd                     |", LIGHT_GREY, &cursor_y);
                    help_row("|   cat                                                                    |", LIGHT_GREY, &cursor_y);
                    help_row("|                                                                          |", WHITE, &cursor_y);
                    help_row("| TIP: Try typing 'help echo'                                              |", YELLOW, &cursor_y);
                    help_row("+--------------------------------------------------------------------------+", BLUE, &cursor_y);

                } else if (strcmp(args, "date") == 0) {
                    print("Usage: date [-t|--time] [-d|--date] [-12|--12h]", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Display RTC date and time.", LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                    print("The flag '-t' or '--time' displays only the time.", LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                    print("The flag '-d' or '--date' displays only the date.", LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                    print("The flag '-12' or '--12h' displays the time in "
                          "12-hour AM/PM format.",
                          LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "echo") == 0) {
                    print("Usage: echo [-c <color>|--color <color>] <text>", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Colors: blue, green, cyan, red, magenta, brown, "
                          "light_grey, ",
                          LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                    print("dark_grey, light_blue, light_green, light_cyan, "
                          "light_red, light_magenta,",
                          LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                    print("yellow, white", LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Print text in a specified color. If no color is "
                          "specified, white is used.",
                          LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "uptime") == 0) {
                    print("Usage: uptime [-p|--pretty] [-s|--seconds]", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Shows elapsed time since boot.", LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                    print("The flag '-p' or '--pretty' shows the elapsed time "
                          "in a more human-readable format.",
                          LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                    print("The flag '-s' or '--seconds' shows the elapsed time "
                          "in seconds.",
                          LIGHT_GREY, 0, cursor_y);

                }  else if (strcmp(args, "clear") == 0) {
                    print("Usage: clear [-dl|--draw-logo]", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Clear screen and reset cursor.", LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                    print("The flag mentioned above clears the screen and "
                          "redraws the logo.",
                          LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "shlogo") == 0) {
                    print("Usage: shlogo", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Display the MyDOS logo.", LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "rbt") == 0) {
                    print("Usage: rbt", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Reboot computer via keyboard controller.", LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "shtdwn") == 0) {
                    print("Usage: shtdwn", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Halts the CPU. Hold the power button to shut down "
                          "your machine.",
                          LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "fault") == 0) {
                    print("Usage: fault <zero|ud>", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Executes real CPU faulting instructions (Divide-by-zero or UD2).", LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "beep") == 0) {
                    print("Usage: beep", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Play a short tone through the legacy PC speaker.",
                          LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "help") == 0) {
                    print("Usage: help [optional: command]", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Show help for all commands or a specific command.", LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "change-theme") == 0) {
                    print("Usage: change-theme <default|matrix|ocean|sunset>", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Changes the color theme.", LIGHT_GREY, 0, cursor_y);
                } else if (strcmp(args, "loadkeys") == 0) {
                    print("Usage: loadkeys <us|de|fr|status>", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Loads a built-in keyboard layout or reports the active layout.", LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "fetch") == 0) {
                    print("Usage: fetch", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Displays system information and uptime beside the MyDOS logo.", LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "mkfile") == 0) {
                    print("Usage: mkfile [FILENAME.EXTENSION]", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Creates a file with the name you chose in the "
                          "current working directory.",
                          LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "ls") == 0) {
                    print("Usage: ls", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Lists files in the current directory.", LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "lsdrv") == 0) {
                    print("Usage: lsdrv", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Lists available storage drives.", LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "mkdir") == 0) {
                    print("Usage: mkdir [DIRNAME]", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Creates a folder with the name you chose in the "
                          "current working directory.",
                          LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "rm") == 0) {
                    print("Usage: rm [/PATH/TO/FILE] / [-r /PATH/TO/DIRECTORY]", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Deletes a file or directory. Please note that this "
                          "is permanent!",
                          LIGHT_GREY, 0, cursor_y);

                } else if (strcmp(args, "cd") == 0) {
                    print("Usage: cd /PATH/TO/DIRECTORY", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Changes the current working directory to your "
                          "specified one.",
                          LIGHT_GREY, 0, cursor_y);
                } else if (strcmp(args, "cat") == 0) {
                    print("Usage: cat <filename>", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Displays the contents of a file.", LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                } else if (strcmp(args, "pwd") == 0) {
                    print("Usage: pwd", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Prints the current working directory.", LIGHT_GREY, 0, cursor_y);
                } else if (strcmp(args, "myeditor") == 0) {
                    print("Usage: myeditor", WHITE, 0, cursor_y);
                    scrl(&cursor_y);
                    print("Opens the built-in MyEditor program.", LIGHT_GREY, 0, cursor_y);
                    scrl(&cursor_y);
                    print("F1 saves, F2 opens a file, and F3 exits the editor.",
                          LIGHT_GREY, 0, cursor_y);
                } else {
                    print("help: unknown command, please try again", RED, 0, cursor_y);
                }
                scrl(&cursor_y);
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

                while (cursor_y + 13 > VGA_HEIGHT) {
                    int scroll_y = VGA_HEIGHT;
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

                static const char block[] = {(char)0xDB, (char)0xDB, '\0'};

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
                print("BEEP! :>", GREEN, 0, cursor_y);
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
                        print("mkfile: File created.", GREEN, 0, cursor_y);
                    } else if (result == FR_EXIST) {
                        print("mkfile: File already exists.", RED, 0, cursor_y);
                    } else {
                        print("mkfile: Could not create file.", RED, 0, cursor_y);
                    }

                    scrl(&cursor_y);
                }
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
                            print("rm: Directory removed.", GREEN, 0, cursor_y);
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
                        print("rm: File removed.", GREEN, 0, cursor_y);
                    } else {
                        print("rm: Could not remove file.", RED, 0, cursor_y);
                    }
                }
                scrl(&cursor_y);
            } else if (strcmp(cmd, "mkdir") == 0) {
                if (args[0] == '\0') {
                    print("Usage: mkdir <dirname>", RED, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
                    FRESULT result = make_dir(args);
                    if (result == FR_OK) {
                        print("mkdir: Directory created.", GREEN, 0, cursor_y);
                    } else if (result == FR_EXIST) {
                        print("mkdir: Directory already exists.", RED, 0, cursor_y);
                    } else {
                        print("mkdir: Could not create directory.", RED, 0, cursor_y);
                    }
                    scrl(&cursor_y);
                }
            } else if (strcmp(cmd, "ls") == 0) {
                if (args[0] != '\0') {
                    print("Usage: ls", RED, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
                    FRESULT result = list_files(".", &cursor_y);

                    if (result != FR_OK) {
                        print("ls: Could not list files.", RED, 0, cursor_y);
                        scrl(&cursor_y);
                    }
                }
            } else if (strcmp(cmd, "lsdrv") == 0) {
                if (args[0] != '\0') {
                    print("Usage: lsdrv", RED, 0, cursor_y);
                } else {
                    list_drives(&cursor_y);
                }
                scrl(&cursor_y);
            } else if (strcmp(cmd, "cd") == 0) {
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
            } else if (strcmp(cmd, "cat") == 0) {
                if (args[0] == '\0') {
                    print("Usage: cat <filename>", RED, 0, cursor_y);
                    scrl(&cursor_y);
                } else {
                    FRESULT res = cat_file(args, &cursor_y);
                    if (res == FR_NO_FILE || res == FR_NO_PATH) {
                        print("cat: File not found.", RED, 0, cursor_y);
                        scrl(&cursor_y);
                    } else if (res != FR_OK) {
                        print("cat: Error reading file.", RED, 0, cursor_y);
                        scrl(&cursor_y);
                    }
                }
            } else if (strcmp(cmd, "myeditor") == 0) {
                myeditor();
                cursor_y = 0;
            } else if (strcmp(cmd, "pwd") == 0) {
                FRESULT result = f_getcwd(cmd, sizeof(cmd));
                if (result == FR_OK) {
                    print(cmd, WHITE, 0, cursor_y);
                } else {
                    print("pwd: Error retrieving current working directory.", RED, 0, cursor_y);
                }
                scrl(&cursor_y);
            }
            else if (strcmp(cmd, "shtdwn") == 0) {
                shutdown();
            } else if (strcmp(cmd, "fault") == 0) {
                if (strcmp(args, "zero") == 0 || strcmp(args, "de") == 0) {
                    volatile int zero = 0;
                    volatile int x = 42 / zero;
                    (void)x;
                } else if (strcmp(args, "ud") == 0) {
                    __asm__ volatile("ud2");
                } else {
                    print("Usage: fault <zero|ud>", RED, 0, cursor_y);
                    scrl(&cursor_y);
                }
            } else if (strcmp(cmd, "rbt") == 0) {
                __asm__ volatile("movb $0xFE, %%al\n\t"
                                 "outb %%al, $0x64\n\t"
                                 :
                                 :
                                 : "al");
            } else if (cmd[0] != '\0') {
                print("trml: unknown command, try again or run 'help' for "
                      "assistance",
                      RED, 0, cursor_y);
                scrl(&cursor_y);
            }
            print("MyDOS", GREEN, 0, cursor_y);
            print("[", RED, 5, cursor_y);
            print("root", LIGHT_RED, 6, cursor_y);
            print("]", RED, 10, cursor_y);
            print("/", LIGHT_RED, 11, cursor_y);
            print(":~#", WHITE, a, cursor_y);
            move_cursor(cursor_x, cursor_y);

            cursor_x = PROMPT_LENGTH;
            cmd_len = 0;

            move_cursor(cursor_x, cursor_y);
        }

        else {
            if (cmd_len < 63) {
                cmd[cmd_len++] = c;

                char str[2] = {c, '\0'};

                print(str, WHITE, cursor_x, cursor_y);

                cursor_x++;
                move_cursor(cursor_x, cursor_y);
            }
        }
    }
}

void scrl(int *cursor_y) {
    (*cursor_y)++;
    scroll_screen(cursor_y);
}