#include <mydos.h>
#include <shell/env.h>

static char shell_path[SHELL_PATH_MAX] = ".:/BIN:/USR/BIN";

const char *shell_env_get_path(void) {
    return shell_path;
}

void shell_env_set_path(const char *path) {
    uint32_t position = 0;
    if (path == 0) {
        path = "";
    }
    while (*path != '\0' && position + 1 < sizeof(shell_path)) {
        shell_path[position++] = *path++;
    }
    shell_path[position] = '\0';
}
