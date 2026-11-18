#include <stdarg.h>
#include <stdlib.h>

#include "doomdef.h"
#include "i_system.h"
#include "i_video.h"
#include "doom_port.h"

#define ZONE_SIZE (16u * 1024u * 1024u)
#define US_PER_TIC 28571u

int mb_used = 16;

static unsigned char zone_memory[ZONE_SIZE] __attribute__((aligned(16)));

static ticcmd_t empty_command;

void I_Init(void) {
    doom_port_micros();
}

byte *I_ZoneBase(int *size) {
    *size = (int)sizeof(zone_memory);
    return zone_memory;
}

int I_GetHeapSize(void) {
    return (int)sizeof(zone_memory);
}

byte *I_AllocLow(int length) {
    return (byte *)malloc((unsigned int)length);
}

int I_GetTime(void) {
    static uint32_t last_microseconds;
    static uint32_t remainder;
    static int tics;
    static int started;

    uint32_t now = (uint32_t)doom_port_micros();
    if (!started) {
        started = 1;
        last_microseconds = now;
        return 0;
    }

    uint32_t delta = now - last_microseconds;
    last_microseconds = now;

    uint32_t scaled = delta + remainder;
    tics += (int)(scaled / US_PER_TIC);
    remainder = scaled % US_PER_TIC;
    return tics;
}

void I_StartFrame(void) {
}


ticcmd_t *I_BaseTiccmd(void) {
    return &empty_command;
}

void I_Tactile(int on, int off, int total) {
    (void)on;
    (void)off;
    (void)total;
}

void I_WaitVBL(int count) {
    if (count > 0) {
        doom_port_delay_us((uint32_t)count * 16683u);
    }
}

void I_Error(char *error, ...) {
    char message[512];
    va_list args;

    va_start(args, error);
    doom_port_vsnprintf(message, sizeof(message), error, args);
    va_end(args);

    doom_port_fatal(message);
}

void I_Quit(void) {
    I_ShutdownGraphics();
    doom_port_exit(0);
}
