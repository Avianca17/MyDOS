#ifndef RTC_H
#define RTC_H

unsigned char rtc_read(unsigned char reg);
unsigned char bcdtb(unsigned char value);
void format_rtc_datetime(char out[32]);

#endif
