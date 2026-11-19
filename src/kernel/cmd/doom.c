#include <cmd/cmd.h>

int doom_main(int argc, char **argv);

static const char *const doom_wad_names[] = {
    "doom1.wad", "doom.wad", "doomu.wad", "doom2.wad",
    "plutonia.wad", "tnt.wad", "freedoom.wad", "freedoom1.wad",
    "freedoom2.wad"
};

static int doom_file_exists(const char *path) {
    FILINFO info;
    return f_stat(path, &info) == FR_OK;
}

static void doom_drive_path(char *out, int drive, const char *name) {
    int i = 0;
    int j = 0;

    out[i++] = (char)('0' + drive);
    out[i++] = ':';
    out[i++] = '/';
    while (name[j] != '\0') {
        out[i++] = name[j++];
    }
    out[i] = '\0';
}

static const char *doom_find_wad(char *out) {
    char path[24];

    for (int drive = 0; drive < FF_VOLUMES; drive++) {
        for (unsigned int i = 0; i < sizeof(doom_wad_names) / sizeof(doom_wad_names[0]); i++) {
            doom_drive_path(path, drive, doom_wad_names[i]);
            if (doom_file_exists(path)) {
                int j = 0;
                while (path[j] != '\0') {
                    out[j] = path[j];
                    j++;
                }
                out[j] = '\0';
                return out;
            }
        }
    }
    return 0;
}

void cmd_doom(char *args, int *cursor_y) {
    char *argv[24];
    int argc = 0;

    argv[argc++] = "doom";
    char *token = cmd_next_token(&args);
    while (token != 0 && argc < 21) {
        argv[argc++] = token;
        token = cmd_next_token(&args);
    }
    argv[argc] = 0;

    int custom_iwad = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-iwad") == 0) {
            if (i + 1 >= argc) {
                print("doom: -iwad needs a WAD filename", RED, 0, *cursor_y);
                scrl(cursor_y);
                return;
            }
            if (!doom_file_exists(argv[i + 1])) {
                print("doom: WAD not found: ", RED, 0, *cursor_y);
                print(argv[i + 1], WHITE, 19, *cursor_y);
                scrl(cursor_y);
                return;
            }
            custom_iwad = 1;
            break;
        }
    }

    if (!custom_iwad) {
        char found[24];
        const char *path = doom_find_wad(found);
        if (path != 0) {
            print("doom: using ", LIGHT_GREY, 0, *cursor_y);
            print(path, WHITE, 12, *cursor_y);
            scrl(cursor_y);
            argv[argc++] = "-iwad";
            argv[argc++] = found;
            argv[argc] = 0;
        } else {
            print("doom: no WAD found in the current directory", RED, 0, *cursor_y);
            scrl(cursor_y);
            print("Put freedoom1.wad here, or run: make doom-disk", LIGHT_GREY, 0, *cursor_y);
            scrl(cursor_y);
            print("Try 'lsdrv' then 'cd 4:/' if the WAD is on another drive.", LIGHT_GREY, 0, *cursor_y);
            scrl(cursor_y);
            return;
        }
    }

    int result = doom_main(argc, argv);

    clear_screen();
    if (result != 0) {
        print("doom: exited with an error", RED, 0, 0);
        *cursor_y = 1;
    } else {
        *cursor_y = 0;
    }
}
