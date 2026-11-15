#include <mydos.h>

void shutdown(void) {
    __asm__ volatile("cli");

    clear_screen();

    // QEMU ONLY
    __asm__ volatile("outw %0, %1"
                     :
                     : "a"((unsigned short)0x2000),
                       "Nd"((unsigned short)0x604));

    for (;;) {
        print("Something went wrong while shutting down. Please shut down manually.", RED, 0, 0);
        __asm__ volatile("hlt");
    }
}
