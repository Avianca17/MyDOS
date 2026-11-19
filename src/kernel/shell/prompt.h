#ifndef MYDOS_SHELL_PROMPT_H
#define MYDOS_SHELL_PROMPT_H

#define SHELL_PROMPT_MAX 96

void shell_prompt_reset(void);
void shell_set_ps1(const char *value);
const char *shell_get_ps1(void);
int shell_prompt_render(int cursor_y, int *cursor_x);

#endif
