#include <cmd/cmd.h>

void cmd_mkfat(char *args, int *cursor_y) {
    char *arguments = args;
    char *format_flag = cmd_next_token(&arguments);
    char *filesystem = cmd_next_token(&arguments);
    char *path = cmd_next_token(&arguments);
    BYTE volume;
    int fat_version;

    if (format_flag == 0 || filesystem == 0 || path == 0 ||
        cmd_next_token(&arguments) != 0 ||
        strcmp(format_flag, "-f") != 0 ||
        !cmd_parse_drive_root(path, &volume)) {
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
