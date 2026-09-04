#pragma once

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define VGA_CTRL 0x3D4
#define VGA_DATA 0x3D5

#define PROMPT_LENGTH 12 // Changed this so that the extra space on the prompt disappeared. 

#define BLACK 0x00
#define BLUE 0x01
#define GREEN 0x02
#define CYAN 0x03
#define RED 0x04
#define MAGENTA 0x05
#define BROWN 0x06
#define LIGHT_GREY 0x07
#define DARK_GREY 0x08
#define LIGHT_BLUE 0x09
#define LIGHT_GREEN 0x0A
#define LIGHT_CYAN 0x0B
#define LIGHT_RED 0x0C
#define LIGHT_MAGENTA 0x0D
#define YELLOW 0x0E
#define WHITE 0x0F

void kernel_main(unsigned int magic, unsigned int mbi);

#include "drivers/vga.h"
#include "lib/string.h"
#include "drivers/rtc.h"
#include "lib/hardware.h"
#include "lib/shtdwn.h"
#include "helper/helpers.h"
