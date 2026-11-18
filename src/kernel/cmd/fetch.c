#include <cmd/cmd.h>
#include <version.h>

void cmd_fetch(char *args, int *cursor_y) {
    (void)args;

    char vendor[13];
    char ram_buf[32];
    char uptime_buf[32];

    unsigned int ram_mb = get_total_ram_mb(cmd_get_mbi());

    cpuid_vendor(vendor);
    itoanum(ram_mb, ram_buf);
    cmd_format_short_uptime(cmd_elapsed_uptime_seconds(), uptime_buf);

    while (*cursor_y + 13 > vga_screen_height()) {
        int scroll_y = vga_screen_height();
        scroll_screen(&scroll_y);
        (*cursor_y)--;
    }

    print_logo_at(0, *cursor_y);
    print("+--KEYS---------+--VALUES----------+", BLUE, 40, *cursor_y);
    print("| User          | root             |", RED, 40, *cursor_y + 1);
    print("| Host          | MyDOS            |", GREEN, 40, *cursor_y + 2);
    print("| Kernel        | ", BLUE, 40, *cursor_y + 3);
    print(KERNEL_VERSION, WHITE, 58, *cursor_y + 3);
    print("|", BLUE, 75, *cursor_y + 3);
    print("| Shell         | ", YELLOW, 40, *cursor_y + 4);
    print(SHELL_VERSION, WHITE, 58, *cursor_y + 4);
    print("|", YELLOW, 75, *cursor_y + 4);
    print("| Uptime        | ", LIGHT_MAGENTA, 40, *cursor_y + 5);
    print(uptime_buf, WHITE, 58, *cursor_y + 5);
    print("|", LIGHT_MAGENTA, 75, *cursor_y + 5);
    print("| Processor     | ", MAGENTA, 40, *cursor_y + 6);
    print(vendor, WHITE, 58, *cursor_y + 6);
    print("|", MAGENTA, 75, *cursor_y + 6);
    print("| Total RAM     | ", LIGHT_BLUE, 40, *cursor_y + 7);
    print(ram_buf, WHITE, 58, *cursor_y + 7);
    print(" MB", WHITE, 58 + strlen(ram_buf), *cursor_y + 7);
    print("|", LIGHT_BLUE, 75, *cursor_y + 7);
    print("+----------------+-----------------+", BLUE, 40, *cursor_y + 8);

    static const char block[] = "**";

    print("+-COLORS---------+", BLUE, 40, *cursor_y + 9);
    print("| ", BLUE, 40, *cursor_y + 10);
    print(block, BLUE, 42, *cursor_y + 10);
    print(block, GREEN, 44, *cursor_y + 10);
    print(block, CYAN, 46, *cursor_y + 10);
    print(block, RED, 48, *cursor_y + 10);
    print(block, MAGENTA, 50, *cursor_y + 10);
    print(block, BROWN, 52, *cursor_y + 10);
    print(block, LIGHT_GREY, 54, *cursor_y + 10);
    print("|", BLUE, 57, *cursor_y + 10);

    print("| ", BLUE, 40, *cursor_y + 11);
    print(block, DARK_GREY, 42, *cursor_y + 11);
    print(block, LIGHT_BLUE, 44, *cursor_y + 11);
    print(block, LIGHT_GREEN, 46, *cursor_y + 11);
    print(block, LIGHT_CYAN, 48, *cursor_y + 11);
    print(block, LIGHT_RED, 50, *cursor_y + 11);
    print(block, LIGHT_MAGENTA, 52, *cursor_y + 11);
    print(block, YELLOW, 54, *cursor_y + 11);
    print("|", BLUE, 57, *cursor_y + 11);
    print("+----------------+", BLUE, 40, *cursor_y + 12);

    *cursor_y += 12;
    scrl(cursor_y);
}
