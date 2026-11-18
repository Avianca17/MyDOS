#include <mydos.h>

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

int shell_show_help(char *args, int cursor_y) {
    if (args[0] != '\0') {
        help_command_header(args, &cursor_y);
    }

    if (args[0] == '\0') {
        help_row("+--------------------------------------------------------------------------+", BLUE, &cursor_y);
        help_row("|                           MYDOS COMMAND CENTRE                           |", GREEN, &cursor_y);
        help_row("+--------------------------------------------------------------------------+", BLUE, &cursor_y);
        help_row("| Want to know what a command does? Just type help <cmd>!                  |", WHITE, &cursor_y);
        help_row("| [ SHELL ]                                                                |", YELLOW, &cursor_y);
        help_row("|   help            clear           echo           change-theme            |", LIGHT_GREY, &cursor_y);
        help_row("|   loadkeys        date            uptime          fetch                  |", LIGHT_GREY, &cursor_y);
        help_row("|   shlogo          beep            larry                                  |", LIGHT_GREY, &cursor_y);
        help_row("| [ NETWORK ]                                                              |", LIGHT_CYAN, &cursor_y);
        help_row("|   dhcp                                                                   |", LIGHT_GREY, &cursor_y);
        help_row("| [ POWER ]                                                                |", LIGHT_CYAN, &cursor_y);
        help_row("|   rbt             shtdwn          kp                                     |", LIGHT_GREY, &cursor_y);
        help_row("| [ FILESYSTEM ]                                                           |", LIGHT_GREEN, &cursor_y);
        help_row("|   mkdir           mkfile          ls             lsdrv                   |", LIGHT_GREY, &cursor_y);
        help_row("|   rm              cd              pwd            mkfat                   |", LIGHT_GREY, &cursor_y);
        help_row("|   read            cp              mv                                     |", LIGHT_GREY, &cursor_y);
        help_row("| [ PROGRAMS ]                                                             |", RED, &cursor_y);
        help_row("|   myeditor        calc                                                   | ", LIGHT_GREY, &cursor_y);
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

    } else if (strcmp(args, "kp") == 0) {
        print("Usage: kp", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Triggers a kernel panic for testing.", LIGHT_GREY, 0, cursor_y);

    } else if (strcmp(args, "beep") == 0) {
        print("Usage: beep", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Play a short tone (This won't work on a lot of VM's/PC's).",
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

    } else if (strcmp(args, "dhcp") == 0) {
        print("Usage: dhcp [renew|status]", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Requests an IPv4 lease from a DHCP server.", LIGHT_GREY, 0, cursor_y);
        scrl(&cursor_y);
        print("Supported NICs: Intel e1000 and Realtek RTL8139/8129.", LIGHT_GREY, 0, cursor_y);

    } else if (strcmp(args, "fetch") == 0) {
        print("Usage: fetch", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Basically fastfetch on Linux/Windows/macOS/BSD, but for MyDOS.", LIGHT_GREY, 0, cursor_y);

    } else if (strcmp(args, "mkfile") == 0) {
        print("Usage: mkfile [FILENAME.EXTENSION]", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Creates a file with the name you chose in the "
              "current working directory.",
              LIGHT_GREY, 0, cursor_y);

    } else if (strcmp(args, "cp") == 0) {
        print("Usage: cp <source> <destination>", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Copies into directories as source filename; never overwrites.", LIGHT_GREY, 0, cursor_y);

    } else if (strcmp(args, "mv") == 0) {
        print("Usage: mv <source> <destination>", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Moves or renames a file or directory; never overwrites.",
              LIGHT_GREY, 0, cursor_y);

    } else if (strcmp(args, "ls") == 0) {
        print("Usage: ls [PATH]", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Lists files in the current directory or a specific path.", LIGHT_GREY, 0, cursor_y);

    } else if (strcmp(args, "lsdrv") == 0) {
        print("Usage: lsdrv", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Lists available drives and their MBR partitions.", LIGHT_GREY, 0, cursor_y);

    } else if (strcmp(args, "mkdir") == 0) {
        print("Usage: mkdir [DIRNAME]", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Creates a folder with the name you chose in the "
              "current working directory.",
              LIGHT_GREY, 0, cursor_y);

    } else if (strcmp(args, "rm") == 0) {
        print("Usage: rm [/PATH/TO/FILE] or  [-r /PATH/TO/DIRECTORY]", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Permanently deletes a file or directory.", LIGHT_GREY, 0, cursor_y);

    } else if (strcmp(args, "cd") == 0) {
        print("Usage: cd /PATH/TO/DIRECTORY", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Changes the current working directory to your "
              "specified one.",
              LIGHT_GREY, 0, cursor_y);
    } else if (strcmp(args, "read") == 0) {
        print("Usage: read <filename>", WHITE, 0, cursor_y);
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
    } else if (strcmp(args, "calc") == 0) {
        print("Usage: calc", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Opens the build-in calculator program.", LIGHT_GREY, 0, cursor_y);
        scrl(&cursor_y);
    } else if (strcmp(args, "mkfat") == 0) {
        print("Usage: mkfat -f <12|16|32> <drive>:/", WHITE, 0, cursor_y);
        scrl(&cursor_y);
        print("Formats the selected disk to the specified FAT version.", LIGHT_GREY, 0, cursor_y);
    } else if (strcmp(args, "larry") == 0) {
        print("Larry", WHITE, 0, cursor_y); scrl(&cursor_y);
        print("emerge --moo", LIGHT_GREY, 0, cursor_y);
    }
    else {
        print("help: unknown command, please try again", RED, 0, cursor_y);
    }
    scrl(&cursor_y);
    return cursor_y;
}
