#include <mydos.h>
#include <net/net_timer.h>

#define NET_PIT_CHANNEL_2 0x42
#define NET_PIT_COMMAND 0x43
#define NET_SPEAKER_CONTROL 0x61
#define NET_TIMER_DIVISOR 11932u

static void net_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint16_t net_timer_read(void) {
    net_outb(NET_PIT_COMMAND, 0x80);
    uint8_t low = io_inb(NET_PIT_CHANNEL_2);
    uint8_t high = io_inb(NET_PIT_CHANNEL_2);
    return (uint16_t)(((uint16_t)high << 8) | low);
}

void net_timer_begin(net_timer_t *timer) {
    uint8_t control = io_inb(NET_SPEAKER_CONTROL);
    net_outb(NET_SPEAKER_CONTROL, (uint8_t)((control & ~0x02u) | 0x01u));
    net_outb(NET_PIT_COMMAND, 0xB4);
    net_outb(NET_PIT_CHANNEL_2, (uint8_t)(NET_TIMER_DIVISOR & 0xFFu));
    net_outb(NET_PIT_CHANNEL_2, (uint8_t)(NET_TIMER_DIVISOR >> 8));
    timer->start = net_timer_read();
    timer->last = timer->start;
    timer->wraps = 0;
}

uint32_t net_timer_elapsed_ms(net_timer_t *timer) {
    uint16_t now = net_timer_read();
    if (now > timer->last) {
        timer->wraps++;
    }
    timer->last = now;
    uint32_t ticks = timer->wraps * NET_TIMER_DIVISOR +
        (uint32_t)((int)timer->start - (int)now);
    return (ticks * 10u) / NET_TIMER_DIVISOR;
}

void net_timer_end(void) {
    uint8_t control = io_inb(NET_SPEAKER_CONTROL);
    net_outb(NET_SPEAKER_CONTROL, (uint8_t)(control & ~0x03u));
}
