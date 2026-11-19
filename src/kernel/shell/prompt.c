#include <mydos.h>
#include "prompt.h"

static char ps1[SHELL_PROMPT_MAX] = "\\h:[\\u]\\$ ";

static void prompt_putc(char character, unsigned char color, int cursor_y,
                        int *cursor_x) {
    if (*cursor_x >= vga_screen_width()) {
        return;
    }

    char text[2] = {character, '\0'};
    print(text, color, *cursor_x, cursor_y);
    (*cursor_x)++;
}

static void prompt_puts(const char *text, unsigned char color, int cursor_y,
                        int *cursor_x) {
    while (*text != '\0') {
        prompt_putc(*text++, color, cursor_y, cursor_x);
    }
}

static const char *cwd_basename(const char *path) {
    const char *base = path;

    for (const char *character = path; *character != '\0'; character++) {
        if (*character == '/' || *character == '\\') {
            base = character + 1;
        }
    }
    return base;
}

void shell_prompt_reset(void) {
    shell_set_ps1("\\h:[\\u]\\$ ");
}

void shell_set_ps1(const char *value) {
    int index = 0;

    if (value == 0 || *value == '\0') {
        shell_set_ps1("\\h:[\\u]\\$ ");
        return;
    }

    while (*value != '\0' && index < SHELL_PROMPT_MAX - 1) {
        ps1[index++] = *value++;
    }
    ps1[index] = '\0';
}

const char *shell_get_ps1(void) {
    return ps1;
}

int shell_prompt_render(int cursor_y, int *cursor_x) {
    char cwd[64];
    if (f_getcwd(cwd, sizeof(cwd)) != FR_OK) {
        cwd[0] = '/';
        cwd[1] = '\0';
    }

    const char *value = ps1;
    while (*value != '\0') {
        if (*value == '\\' && value[1] != '\0') {
            value++;
            switch (*value) {
                case 'h':
                    prompt_puts("MyDOS", GREEN, cursor_y, cursor_x);
                    break;
                case 'u':
                    prompt_puts("root", LIGHT_RED, cursor_y, cursor_x);
                    break;
                case 'w':
                    prompt_puts(cwd, LIGHT_CYAN, cursor_y, cursor_x);
                    break;
                case 'W':
                    prompt_puts(cwd_basename(cwd), LIGHT_CYAN, cursor_y, cursor_x);
                    break;
                case '$':
                    prompt_puts("#", WHITE, cursor_y, cursor_x);
                    break;
                case '\\':
                    prompt_puts("\\", WHITE, cursor_y, cursor_x);
                    break;
                default:
                    prompt_putc('\\', WHITE, cursor_y, cursor_x);
                    prompt_putc(*value, WHITE, cursor_y, cursor_x);
                    break;
            }
            value++;
        } else {
            prompt_putc(*value++, WHITE, cursor_y, cursor_x);
        }
    }

    return *cursor_x;
}
