#ifndef MYDOS_USER_API_H
#define MYDOS_USER_API_H

#include <stdint.h>

typedef void (*mydos_print_fn)(const char *message, uint8_t color, int x, int y);
typedef void (*mydos_scroll_fn)(int *cursor_y);
typedef char (*mydos_getchar_fn)(void);

typedef struct {
    uint32_t version;
    mydos_print_fn print;
    mydos_scroll_fn scroll;
    mydos_getchar_fn getchar;
} mydos_user_api_t;

typedef int (*mydos_user_entry_t)(int argc, char **argv,
                                  const mydos_user_api_t *api);

#endif
