#include <cmd/cmd.h>

typedef struct {
    const char *lines[3];
    unsigned int line_count;
} larry_message_t;

static const larry_message_t larry_messages[] = {
    {{" < Live, Love, Compile! >", 0, 0}, 1},
    {{" < Have you mooed today? >", 0, 0}, 1},
    {{" < Hello, world! >", 0, 0}, 1},
    {{" < Happy coding, everyone! >", 0, 0}, 1},
    {{
        " < Uh, I see his balls, Juicy >",
        " <      - Gigsteak15          >"
    }, 2},
    {{" < Gentoo is when opsec btw >", 0, 0}, 1},
    {{" < doas emerge --ask kernel/mydos >", 0, 0}, 1},
    {{" < tree  - v1sta_ >", 0, 0}, 1},
    {{" < gentoo is a gentoo  - gentoo cunt >", 0, 0}, 1},
    {{
        " <     New to MyDOS? Don't be creeped out by the lack     >",
        " <       of GUI! It's a very simple OS that even a        >",
        " < child could use! Type 'help' if you ever get confused. >"
    }, 3},
    {{
        " < There is a pipe bomb at your door >",
        " <           - Pitbull               >"
    }, 2},
    {{
        " < Message from the owner of MyDOS: >",
        " < Gentoo is the best Linux distro  >",
        " < and you can't change my mind!    >"
    }, 3},
    {{
        " < larp is when netbsd >",
        " <     - yaffs2        >"
    }, 2},
    {{" < the - Boingboing737 >", 0, 0}, 1},
    {{
        " < INSTALL VISNUX >",
        " <    - v1sta_     >"
    }, 2},
    {{" < Happy birthday, pluto! >", 0, 0}, 1},
    {{" < Install Gentoo Linux on your PC today! >", 0, 0}, 1},
    {{" < I wish I could get a free bag of chips... >", 0, 0}, 1},
    {{
        " < I only eat compost >",
        " <    - Poko_Banana   >"
    }, 2},
    {{
        " < Thank you blyat suka suka suka thank >",
        " <      you for subscribe and like      >",
        " <           - danielka552              >"
    }, 3},
    {{
        " < its for my kernel >",
        " <    - mayainit     >"
    }, 2},
};

void cmd_larry(char *args, int *cursor_y) {
    static unsigned int larry_message_index;
    const larry_message_t *message = &larry_messages[larry_message_index];
    int border_width = 0;

    (void)args;

    for (unsigned int i = 0; i < message->line_count; i++) {
        int line_width = strlen(message->lines[i]);
        if (line_width > border_width) {
            border_width = line_width;
        }
    }
    border_width += 2;
    if (border_width > vga_screen_width()) {
        border_width = vga_screen_width();
    }
    border_width -= 3;

    char border[VGA_MAX_WIDTH + 1];
    border[0] = ' ';
    border[1] = ' ';
    for (int i = 2; i < border_width; i++) {
        border[i] = '_';
    }
    border[border_width] = '\0';

    print("Larry loves Gentoo (Linux)", WHITE, 0, *cursor_y);
    scrl(cursor_y);
    scrl(cursor_y);
    print(border, LIGHT_GREY, 0, *cursor_y);
    scrl(cursor_y);
    for (unsigned int i = 0; i < message->line_count; i++) {
        print(message->lines[i], LIGHT_GREY, 0, *cursor_y);
        scrl(cursor_y);
    }
    for (int i = 2; i < border_width; i++) {
        border[i] = '-';
    }
    print(border, LIGHT_GREY, 0, *cursor_y);
    scrl(cursor_y);
    print("        \\   ^__^", LIGHT_GREY, 0, *cursor_y);
    scrl(cursor_y);
    print("         \\  (oo)\\_______", LIGHT_GREY, 0, *cursor_y);
    scrl(cursor_y);
    print("            (__)\\        )\\/\\", LIGHT_GREY, 0, *cursor_y);
    scrl(cursor_y);
    print("                ||----w |", LIGHT_GREY, 0, *cursor_y);
    scrl(cursor_y);
    print("                ||     ||", LIGHT_GREY, 0, *cursor_y);
    scrl(cursor_y);
    scrl(cursor_y);

    larry_message_index =
        (larry_message_index + 1) %
        (sizeof(larry_messages) / sizeof(larry_messages[0]));
}
