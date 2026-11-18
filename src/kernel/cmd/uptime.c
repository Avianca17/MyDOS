#include <cmd/cmd.h>

static unsigned int boot_seconds;
static int uptime_initialized;

void cmd_uptime_init(void) {
    unsigned char sec = bcdtb(rtc_read(0x00));
    unsigned char min = bcdtb(rtc_read(0x02));
    unsigned char hr = bcdtb(rtc_read(0x04));

    boot_seconds = rtcts(hr, min, sec);
    uptime_initialized = 1;
}

unsigned int cmd_elapsed_uptime_seconds(void) {
    if (!uptime_initialized) {
        cmd_uptime_init();
    }

    unsigned char sec = bcdtb(rtc_read(0x00));
    unsigned char min = bcdtb(rtc_read(0x02));
    unsigned char hr = bcdtb(rtc_read(0x04));
    unsigned int now_seconds = rtcts(hr, min, sec);

    if (now_seconds < boot_seconds) {
        now_seconds += 86400u;
    }

    return now_seconds - boot_seconds;
}

void cmd_format_short_uptime(unsigned int elapsed, char *out) {
    unsigned int days = elapsed / 86400u;
    unsigned int hours = (elapsed % 86400u) / 3600u;
    unsigned int minutes = (elapsed % 3600u) / 60u;
    unsigned int seconds = elapsed % 60u;
    int pos = 0;

    if (days > 0) {
        char days_buf[16];
        append_uint(days_buf, days);
        for (int i = 0; days_buf[i] != '\0'; i++) {
            out[pos++] = days_buf[i];
        }
        out[pos++] = 'd';
        out[pos++] = ' ';
    }

    out[pos++] = (char)('0' + hours / 10);
    out[pos++] = (char)('0' + hours % 10);
    out[pos++] = ':';
    out[pos++] = (char)('0' + minutes / 10);
    out[pos++] = (char)('0' + minutes % 10);
    out[pos++] = ':';
    out[pos++] = (char)('0' + seconds / 10);
    out[pos++] = (char)('0' + seconds % 10);
    out[pos] = '\0';
}

void cmd_uptime(char *args, int *cursor_y) {
    unsigned int elapsed = cmd_elapsed_uptime_seconds();

    char uptime_str[64];
    int pos = 0;
    unsigned int d = elapsed / 86400u;
    unsigned int h = (elapsed % 86400u) / 3600u;
    unsigned int m = (elapsed % 3600u) / 60u;
    unsigned int s = elapsed % 60u;

    int pretty = (strcmp(args, "-p") == 0) || (strcmp(args, "--pretty") == 0);

    if (pretty) {
        char value_buf[16];

        const char *prefix = "up for ";
        for (int i = 0; prefix[i] != '\0'; i++) {
            uptime_str[pos++] = prefix[i];
        }

        append_uint(value_buf, d);
        for (int i = 0; value_buf[i] != '\0'; i++) {
            uptime_str[pos++] = value_buf[i];
        }
        const char *days_label = " days ";
        for (int i = 0; days_label[i] != '\0'; i++) {
            uptime_str[pos++] = days_label[i];
        }

        append_uint(value_buf, h);
        for (int i = 0; value_buf[i] != '\0'; i++) {
            uptime_str[pos++] = value_buf[i];
        }
        const char *hours_label = " hours ";
        for (int i = 0; hours_label[i] != '\0'; i++) {
            uptime_str[pos++] = hours_label[i];
        }

        append_uint(value_buf, m);
        for (int i = 0; value_buf[i] != '\0'; i++) {
            uptime_str[pos++] = value_buf[i];
        }
        const char *minutes_label = " minutes and ";
        for (int i = 0; minutes_label[i] != '\0'; i++) {
            uptime_str[pos++] = minutes_label[i];
        }

        append_uint(value_buf, s);
        for (int i = 0; value_buf[i] != '\0'; i++) {
            uptime_str[pos++] = value_buf[i];
        }
        const char *seconds_label = " seconds";
        for (int i = 0; seconds_label[i] != '\0'; i++) {
            uptime_str[pos++] = seconds_label[i];
        }
    } else if (strcmp(args, "-s") == 0 || strcmp(args, "--seconds") == 0) {
        char sec_buf[32];
        append_uint(sec_buf, elapsed);
        for (int i = 0; sec_buf[i] != '\0'; i++) {
            uptime_str[pos++] = sec_buf[i];
        }
        const char *label = " seconds";
        for (int i = 0; label[i] != '\0'; i++) {
            uptime_str[pos++] = label[i];
        }
    } else {
        if (d > 0) {
            char d_buf[16];
            append_uint(d_buf, d);
            for (int i = 0; d_buf[i] != '\0'; i++) {
                uptime_str[pos++] = d_buf[i];
            }
            uptime_str[pos++] = 'd';
            uptime_str[pos++] = ' ';
        }

        uptime_str[pos++] = (char)(h / 10 + '0');
        uptime_str[pos++] = (char)(h % 10 + '0');
        uptime_str[pos++] = ':';
        uptime_str[pos++] = (char)(m / 10 + '0');
        uptime_str[pos++] = (char)(m % 10 + '0');
        uptime_str[pos++] = ':';
        uptime_str[pos++] = (char)(s / 10 + '0');
        uptime_str[pos++] = (char)(s % 10 + '0');
    }

    uptime_str[pos] = '\0';
    print(uptime_str, WHITE, 0, *cursor_y);
    scrl(cursor_y);
}
