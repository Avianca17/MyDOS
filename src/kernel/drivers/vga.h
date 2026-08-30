#ifndef CONSOLE_H
#define CONSOLE_H

void move_cursor(int x, int y);
void print_logo_at(int x, int y);
void print_logo(void);
void print(const char* msg, unsigned char color, int x, int y);
void clear_screen(void);
void scroll_screen(int* cursor_y);

#endif
