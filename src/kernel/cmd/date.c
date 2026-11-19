#include <cmd/cmd.h>

void cmd_date(char *args, int *cursor_y) {
    char datetime[64];

    if (strcmp(args, "-t") == 0 || strcmp(args, "--time") == 0) {
        format_rtc_time(datetime, 0);
    } else if (strcmp(args, "-d") == 0 || strcmp(args, "--date") == 0) {
        format_rtc_date_only(datetime);
    } else if (strcmp(args, "-12") == 0 || strcmp(args, "--12h") == 0) {
        format_rtc_time(datetime, 1);
    } else if (strcmp(args, "-h") == 0 || strcmp(args, "--help") == 0) {
        print("Usage: date [-t|--time] [-d|--date] [-12|--12h]", WHITE, 0, *cursor_y);
        scrl(cursor_y);
        return;
    } else {
        format_rtc_datetime(datetime);
    }

    print(datetime, WHITE, 0, *cursor_y);
    scrl(cursor_y);
}
