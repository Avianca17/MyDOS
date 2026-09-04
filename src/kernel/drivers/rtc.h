#ifndef RTC_H
#define RTC_H

unsigned char rtc_read(unsigned char reg);
unsigned char bcdtb(unsigned char value);
void format_rtc_datetime(char out[32]);
void format_rtc_time(char out[32], int format_12h);
void format_rtc_date_only(char out[32]);

#endif
