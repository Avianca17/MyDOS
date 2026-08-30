#include "kernel.h"
#include "rtc.h"
#include "drivers/keyboard.h"

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

unsigned char rtc_read(unsigned char reg) {
    outb(0x70, reg);
    return inb(0x71);
}

unsigned char bcdtb(unsigned char value) {
    return (value & 0x0F) + ((value >> 4) * 10);
}

void format_rtc_datetime(char out[32]) {
    unsigned char second = bcdtb(rtc_read(0x00));
    unsigned char minute = bcdtb(rtc_read(0x02));
    unsigned char hour = bcdtb(rtc_read(0x04));
    unsigned char day = bcdtb(rtc_read(0x07));
    unsigned char month = bcdtb(rtc_read(0x08));
    unsigned char year = bcdtb(rtc_read(0x09));

    int pos = 0;

    out[pos++] = (char)('0' + (hour / 10));
    out[pos++] = (char)('0' + (hour % 10));
    out[pos++] = ':';
    out[pos++] = (char)('0' + (minute / 10));
    out[pos++] = (char)('0' + (minute % 10));
    out[pos++] = ':';
    out[pos++] = (char)('0' + (second / 10));
    out[pos++] = (char)('0' + (second % 10));
    out[pos++] = ',';
    out[pos++] = ' ';
    out[pos++] = (char)('0' + (day / 10));
    out[pos++] = (char)('0' + (day % 10));
    out[pos++] = '/';
    out[pos++] = (char)('0' + (month / 10));
    out[pos++] = (char)('0' + (month % 10));
    out[pos++] = '/';
    out[pos++] = (char)('0' + (year / 10));
    out[pos++] = (char)('0' + (year % 10));
    out[pos] = '\0';
}
