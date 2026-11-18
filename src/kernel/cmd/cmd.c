#include <cmd/cmd.h>

typedef struct cmd_entry {
    const char *name;
    cmd_fn_t run;
} cmd_entry_t;

static unsigned int multiboot_mbi;

static const cmd_entry_t commands[] = {
    {"beep", cmd_beep},
    {"calc", cmd_calc},
    {"cd", cmd_cd},
    {"change-theme", cmd_change_theme},
    {"clear", cmd_clear},
    {"cp", cmd_cp},
    {"date", cmd_date},
    {"dhcp", cmd_dhcp},
    {"doom", cmd_doom},
    {"echo", cmd_echo},
    {"fetch", cmd_fetch},
    {"help", cmd_help},
    {"kp", cmd_kp},
    {"larry", cmd_larry},
    {"loadkeys", cmd_loadkeys},
    {"ls", cmd_ls},
    {"lsdrv", cmd_lsdrv},
    {"mk-mbr-part", cmd_mk_mbr_part},
    {"mkdir", cmd_mkdir},
    {"mkfat", cmd_mkfat},
    {"mkfile", cmd_mkfile},
    {"mv", cmd_mv},
    {"myeditor", cmd_myeditor},
    {"ping", cmd_ping},
    {"pwd", cmd_pwd},
    {"rbt", cmd_rbt},
    {"read", cmd_read},
    {"rm", cmd_rm},
    {"shlogo", cmd_shlogo},
    {"shtdwn", cmd_shtdwn},
    {"uptime", cmd_uptime},
};

void cmd_set_mbi(unsigned int mbi) {
    multiboot_mbi = mbi;
}

unsigned int cmd_get_mbi(void) {
    return multiboot_mbi;
}

int cmd_run(const char *name, char *args, int *cursor_y) {
    for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(name, commands[i].name) == 0) {
            commands[i].run(args, cursor_y);
            return 1;
        }
    }

    return 0;
}

char *cmd_next_token(char **input) {
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

int cmd_parse_drive_root(const char *path, BYTE *volume) {
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
