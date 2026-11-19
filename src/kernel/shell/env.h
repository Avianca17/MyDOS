#ifndef MYDOS_SHELL_ENV_H
#define MYDOS_SHELL_ENV_H

#define SHELL_PATH_MAX 192

const char *shell_env_get_path(void);
void shell_env_set_path(const char *path);

#endif
