#include <cmd/cmd.h>

void cmd_beep(char *args, int *cursor_y) {
    (void)args;

    pc_speaker_beep();
    print("BEEP! :)", GREEN, 0, *cursor_y);
    scrl(cursor_y);
}
