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

void format_rtc_time(char out[32], int format_12h) {
    unsigned char second = bcdtb(rtc_read(0x00));
    unsigned char minute = bcdtb(rtc_read(0x02));
    unsigned char hour = bcdtb(rtc_read(0x04));

    int is_pm = 0;
    if (format_12h) {
        if (hour >= 12) {
            is_pm = 1;
            if (hour > 12) hour -= 12;
        } else if (hour == 0) {
            hour = 12;
        }
    }

    int pos = 0;
    out[pos++] = (char)('0' + (hour / 10));
    out[pos++] = (char)('0' + (hour % 10));
    out[pos++] = ':';
    out[pos++] = (char)('0' + (minute / 10));
    out[pos++] = (char)('0' + (minute % 10));
    out[pos++] = ':';
    out[pos++] = (char)('0' + (second / 10));
    out[pos++] = (char)('0' + (second % 10));

    if (format_12h) {
        out[pos++] = ' ';
        out[pos++] = is_pm ? 'P' : 'A';
        out[pos++] = 'M';
    }

    out[pos] = '\0';
}

void format_rtc_date_only(char out[32]) {
    unsigned char day = bcdtb(rtc_read(0x07));
    unsigned char month = bcdtb(rtc_read(0x08));
    unsigned char year = bcdtb(rtc_read(0x09));

    int pos = 0;
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
