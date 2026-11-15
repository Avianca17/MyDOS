#include <mydos.h>

#define PIT_CHANNEL_2 0x42
#define PIT_COMMAND 0x43
#define PC_SPEAKER_PORT 0x61
#define PIT_FREQUENCY 1193182u
#define BEEP_FREQUENCY 1000u
#define BEEP_TRANSITIONS 200u
#define BEEP_POLL_LIMIT 10000000u

static unsigned char speaker_inb(unsigned short port) {
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

void pc_speaker_beep(void) {
    unsigned short divisor = (unsigned short)(PIT_FREQUENCY / BEEP_FREQUENCY);

    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CHANNEL_2, (unsigned char)(divisor & 0xFF));
    outb(PIT_CHANNEL_2, (unsigned char)(divisor >> 8));

    unsigned char speaker = speaker_inb(PC_SPEAKER_PORT);
    outb(PC_SPEAKER_PORT, speaker | 0x03);

    unsigned char previous = speaker_inb(PC_SPEAKER_PORT) & 0x20;
    unsigned int transitions = 0;
    for (unsigned int poll = 0;
         poll < BEEP_POLL_LIMIT && transitions < BEEP_TRANSITIONS;
         poll++) {
        unsigned char current = speaker_inb(PC_SPEAKER_PORT) & 0x20;
        if (current != previous) {
            previous = current;
            transitions++;
        }
    }

    outb(PC_SPEAKER_PORT,
         speaker_inb(PC_SPEAKER_PORT) & (unsigned char)~0x03);
}
