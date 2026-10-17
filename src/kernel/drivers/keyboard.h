#ifndef KEYBOARD_H
#define KEYBOARD_H

unsigned char inb(unsigned short port);
char keyboard_getchar(void);
int keyboard_load_layout(const char *layout_name);
const char *keyboard_get_layout_name(void);

/* PS/2 Scan Codes (Set 1) for Function Keys */
#define SCANCODE_F1   0x3B
#define SCANCODE_F2   0x3C
#define SCANCODE_F3   0x3D
#define SCANCODE_F4   0x3E
#define SCANCODE_F5   0x3F
#define SCANCODE_F6   0x40
#define SCANCODE_F7   0x41
#define SCANCODE_F8   0x42
#define SCANCODE_F9   0x43
#define SCANCODE_F10  0x44
#define SCANCODE_F11  0x57
#define SCANCODE_F12  0x58

/* Box-Drawing Characters (Code Page 437) - Double-Line */
#define BOX_DOUBLE_HORIZONTAL       ((char)0xCD) /* ═ */
#define BOX_DOUBLE_VERTICAL         ((char)0xBA) /* ║ */
#define BOX_DOUBLE_DOWN_RIGHT       ((char)0xC9) /* ╔ */
#define BOX_DOUBLE_DOWN_LEFT        ((char)0xBB) /* ╗ */
#define BOX_DOUBLE_UP_RIGHT         ((char)0xC8) /* ╚ */
#define BOX_DOUBLE_UP_LEFT          ((char)0xBC) /* ╝ */
#define BOX_DOUBLE_VERTICAL_RIGHT   ((char)0xCC) /* ╠ */
#define BOX_DOUBLE_VERTICAL_LEFT    ((char)0xB9) /* ╣ */
#define BOX_DOUBLE_DOWN_HORIZONTAL  ((char)0xCB) /* ╦ */
#define BOX_DOUBLE_UP_HORIZONTAL    ((char)0xCA) /* ╩ */
#define BOX_DOUBLE_CROSS            ((char)0xCE) /* ╬ */

/* Box-Drawing Characters (Code Page 437) - Single-Line */
#define BOX_SINGLE_HORIZONTAL       ((char)0xC4) /* ─ */
#define BOX_SINGLE_VERTICAL         ((char)0xB3) /* │ */
#define BOX_SINGLE_DOWN_RIGHT       ((char)0xDA) /* ┌ */
#define BOX_SINGLE_DOWN_LEFT        ((char)0xBF) /* ┐ */
#define BOX_SINGLE_UP_RIGHT         ((char)0xC0) /* └ */
#define BOX_SINGLE_UP_LEFT          ((char)0xD9) /* ┘ */
#define BOX_SINGLE_VERTICAL_RIGHT   ((char)0xC3) /* ├ */
#define BOX_SINGLE_VERTICAL_LEFT    ((char)0xB4) /* ┤ */
#define BOX_SINGLE_DOWN_HORIZONTAL  ((char)0xC2) /* ┬ */
#define BOX_SINGLE_UP_HORIZONTAL    ((char)0xC1) /* ┴ */
#define BOX_SINGLE_CROSS            ((char)0xC5) /* ┼ */

/* Short aliases for box-drawing characters */
#define BOX_D_HORIZ     BOX_DOUBLE_HORIZONTAL
#define BOX_D_VERT      BOX_DOUBLE_VERTICAL
#define BOX_D_TOPLEFT   BOX_DOUBLE_DOWN_RIGHT
#define BOX_D_TOPRIGHT  BOX_DOUBLE_DOWN_LEFT
#define BOX_D_BOTLEFT   BOX_DOUBLE_UP_RIGHT
#define BOX_D_BOTRIGHT  BOX_DOUBLE_UP_LEFT
#define BOX_D_TLEFT     BOX_DOUBLE_VERTICAL_RIGHT
#define BOX_D_TRIGHT    BOX_DOUBLE_VERTICAL_LEFT
#define BOX_D_TTOP      BOX_DOUBLE_DOWN_HORIZONTAL
#define BOX_D_TBOT      BOX_DOUBLE_UP_HORIZONTAL
#define BOX_D_CROSS     BOX_DOUBLE_CROSS

#define BOX_S_HORIZ     BOX_SINGLE_HORIZONTAL
#define BOX_S_VERT      BOX_SINGLE_VERTICAL
#define BOX_S_TOPLEFT   BOX_SINGLE_DOWN_RIGHT
#define BOX_S_TOPRIGHT  BOX_SINGLE_DOWN_LEFT
#define BOX_S_BOTLEFT   BOX_SINGLE_UP_RIGHT
#define BOX_S_BOTRIGHT  BOX_SINGLE_UP_LEFT
#define BOX_S_TLEFT     BOX_SINGLE_VERTICAL_RIGHT
#define BOX_S_TRIGHT    BOX_SINGLE_VERTICAL_LEFT
#define BOX_S_TTOP      BOX_SINGLE_DOWN_HORIZONTAL
#define BOX_S_TBOT      BOX_SINGLE_UP_HORIZONTAL
#define BOX_S_CROSS     BOX_SINGLE_CROSS

/* Function key character mappings returned by keyboard_getchar() */
#define KEY_F1          BOX_DOUBLE_HORIZONTAL
#define KEY_F2          BOX_DOUBLE_VERTICAL
#define KEY_F3          BOX_DOUBLE_DOWN_RIGHT
#define KEY_F4          BOX_DOUBLE_DOWN_LEFT
#define KEY_F5          BOX_DOUBLE_UP_RIGHT
#define KEY_F6          BOX_DOUBLE_UP_LEFT
#define KEY_F7          BOX_DOUBLE_VERTICAL_RIGHT
#define KEY_F8          BOX_DOUBLE_VERTICAL_LEFT
#define KEY_F9          BOX_DOUBLE_DOWN_HORIZONTAL
#define KEY_F10         BOX_DOUBLE_UP_HORIZONTAL
#define KEY_F11         BOX_DOUBLE_CROSS
#define KEY_F12         ((char)0x80)
#define KEY_ARROW_UP    ((char)0xF0)
#define KEY_ARROW_DOWN  ((char)0xF1)
#define KEY_ARROW_LEFT  ((char)0xF2)
#define KEY_ARROW_RIGHT ((char)0xF3)

#define KEY_SHIFT_F1    BOX_SINGLE_HORIZONTAL
#define KEY_SHIFT_F2    BOX_SINGLE_VERTICAL
#define KEY_SHIFT_F3    BOX_SINGLE_DOWN_RIGHT
#define KEY_SHIFT_F4    BOX_SINGLE_DOWN_LEFT
#define KEY_SHIFT_F5    BOX_SINGLE_UP_RIGHT
#define KEY_SHIFT_F6    BOX_SINGLE_UP_LEFT
#define KEY_SHIFT_F7    BOX_SINGLE_VERTICAL_RIGHT
#define KEY_SHIFT_F8    BOX_SINGLE_VERTICAL_LEFT
#define KEY_SHIFT_F9    BOX_SINGLE_DOWN_HORIZONTAL
#define KEY_SHIFT_F10   BOX_SINGLE_UP_HORIZONTAL
#define KEY_SHIFT_F11   BOX_SINGLE_CROSS
#define KEY_SHIFT_F12   ((char)0x80)

#endif