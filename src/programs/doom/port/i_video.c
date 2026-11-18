#include <string.h>

#include "doomdef.h"
#include "d_main.h"
#include "i_video.h"
#include "i_system.h"
#include "v_video.h"
#include "doom_port.h"

static int scancode_keys[128];

static int key_from_event(int key, int scancode) {
    if (key == DOOM_PORT_KEY_SHIFT) return KEY_RSHIFT;
    if (key == DOOM_PORT_KEY_CTRL) return KEY_RCTRL;
    if (key == DOOM_PORT_KEY_ALT) return KEY_RALT;

    switch (scancode) {
        case 0x3B: return KEY_F1;
        case 0x3C: return KEY_F2;
        case 0x3D: return KEY_F3;
        case 0x3E: return KEY_F4;
        case 0x3F: return KEY_F5;
        case 0x40: return KEY_F6;
        case 0x41: return KEY_F7;
        case 0x42: return KEY_F8;
        case 0x43: return KEY_F9;
        case 0x44: return KEY_F10;
        case 0x57: return KEY_F11;
        case 0x58: return KEY_F12;
        case 0x48: return KEY_UPARROW;
        case 0x50: return KEY_DOWNARROW;
        case 0x4B: return KEY_LEFTARROW;
        case 0x4D: return KEY_RIGHTARROW;
        default: break;
    }

    if (key == 0x1B) return KEY_ESCAPE;
    if (key == '\n' || key == '\r') return KEY_ENTER;
    if (key == '\t') return KEY_TAB;
    if (key == '\b') return KEY_BACKSPACE;

    if (key >= 'A' && key <= 'Z') {
        key += 'a' - 'A';
    }
    if (key >= 32 && key < 127) {
        return key;
    }
    return 0;
}

void I_InitGraphics(void) {
    if (!doom_port_framebuffer_ready()) {
        I_Error("DOOM needs a linear framebuffer.\n");
    }
}

void I_ShutdownGraphics(void) {
}

void I_SetPalette(byte *palette) {
    doom_port_set_palette(palette);
}

void I_UpdateNoBlit(void) {
}

void I_FinishUpdate(void) {
    doom_port_present(screens[0]);
}

void I_ReadScreen(byte *screen) {
    memcpy(screen, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

void I_BeginRead(void) {
}

void I_EndRead(void) {
}

void I_StartTic(void) {
    int key;
    int pressed;
    int scancode;

    while (doom_port_key_poll(&key, &pressed, &scancode)) {
        int mapped = 0;

        if (scancode < 0 || scancode >= 128) {
            continue;
        }

        if (pressed) {
            mapped = key_from_event(key, scancode);
            if (mapped != 0) {
                scancode_keys[scancode] = mapped;
            }
        } else {
            mapped = scancode_keys[scancode];
            scancode_keys[scancode] = 0;
            if (mapped == 0) {
                mapped = key_from_event(key, scancode);
            }
        }

        if (mapped == 0) {
            continue;
        }

        event_t event;
        event.type = pressed ? ev_keydown : ev_keyup;
        event.data1 = mapped;
        event.data2 = 0;
        event.data3 = 0;
        D_PostEvent(&event);
    }
}
