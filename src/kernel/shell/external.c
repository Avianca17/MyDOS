#include <mydos.h>
#include <cmd/cmd.h>
#include <elf/elf.h>
#include <ff.h>
#include <shell/env.h>
#include <shell/external.h>

#define EXTERNAL_MAX_ARGS 16
#define EXTERNAL_PATH_MAX 192

static int external_append(char *out, uint32_t capacity, uint32_t *position,
                           const char *text) {
    while (*text != '\0') {
        if (*position + 1 >= capacity) {
            return 0;
        }
        out[(*position)++] = *text++;
    }
    return 1;
}

static void external_line(const char *text, unsigned char color, int *cursor_y) {
    print(text, color, 0, *cursor_y);
    scrl(cursor_y);
}

static int external_build_path(char *out, uint32_t capacity,
                               const char *entry, uint32_t entry_length,
                               const char *drive, const char *name,
                               int with_extension) {
    uint32_t position = 0;
    int use_drive = 1;

    if (entry_length > 0 && entry[0] == '.') {
        use_drive = 0;
    }
    if (entry_length > 1 && entry[1] == ':') {
        use_drive = 0;
    }

    if (use_drive && !external_append(out, capacity, &position, drive)) {
        return 0;
    }
    for (uint32_t i = 0; i < entry_length; i++) {
        if (position + 1 >= capacity) {
            return 0;
        }
        out[position++] = entry[i];
    }
    if (entry_length == 0 || entry[entry_length - 1] != '/') {
        if (!external_append(out, capacity, &position, "/")) {
            return 0;
        }
    }
    if (!external_append(out, capacity, &position, name)) {
        return 0;
    }
    if (with_extension && !external_append(out, capacity, &position, ".ELF")) {
        return 0;
    }
    out[position] = '\0';
    return 1;
}

static int external_find(const char *name, char *out, uint32_t capacity) {
    char cwd[128];
    char drive[4] = "0:";
    if (f_getcwd(cwd, sizeof(cwd)) == FR_OK && cwd[1] == ':') {
        drive[0] = cwd[0];
        drive[1] = ':';
    }

    const char *path = shell_env_get_path();
    while (*path != '\0') {
        const char *separator = strchr(path, ':');
        uint32_t length = separator != 0 ? (uint32_t)(separator - path)
                                         : (uint32_t)strlen(path);
        FILINFO info;
        if (length != 0 && length < 128 &&
            external_build_path(out, capacity, path, length, drive, name, 0) &&
            f_stat(out, &info) == FR_OK) {
            return 1;
        }
        if (length != 0 && length < 128 &&
            external_build_path(out, capacity, path, length, drive, name, 1) &&
            f_stat(out, &info) == FR_OK) {
            return 1;
        }
        if (separator == 0) {
            break;
        }
        path = separator + 1;
    }
    return 0;
}

int shell_run_external(const char *name, char *args, int *cursor_y) {
    char path[EXTERNAL_PATH_MAX];
    if (!external_find(name, path, sizeof(path))) {
        return 0;
    }

    char *argv[EXTERNAL_MAX_ARGS];
    int argument_count = 0;
    argv[argument_count++] = (char *)name;
    char *input = args;
    while (argument_count < EXTERNAL_MAX_ARGS) {
        char *token = cmd_next_token(&input);
        if (token == 0) {
            break;
        }
        argv[argument_count++] = token;
    }

    int exit_code = 0;
    if (!elf_run(path, cmd_get_mbi(), argument_count, argv, &exit_code)) {
        external_line("shell: could not load ", RED, cursor_y);
        print(path, WHITE, 22, *cursor_y - 1);
        return 1;
    }
    return 1;
}
