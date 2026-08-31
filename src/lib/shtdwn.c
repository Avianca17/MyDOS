#include "shtdwn.h"
#include "../kernel/drivers/vga.h"

void shutdown() {
    // Disable all interrupts for safe shutdown
    __asm__ volatile("cli");
    
    // Clear screen and display shutdown message
    clear_screen();
    print("It is now safe to power off your computer.", 0x0C, 15, 12);

    // Halt the CPU to stop execution
    while (1) {
        __asm__ volatile("hlt");
    }
}