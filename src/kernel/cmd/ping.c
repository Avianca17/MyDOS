#include <cmd/cmd.h>
#include <net/net.h>

static void ping_line_clear(int y) {
    print("                                                                               ",
          WHITE, 0, y);
}

static void print_ping_reply(const net_ping_reply_t *reply, int *cursor_y) {
    char address[16];
    char number[16];
    char time_text[96];
    int position = 0;

    net_format_ip(reply->address, address);
    append_uint(number, reply->bytes);

    const char *from = " bytes from ";
    for (int i = 0; from[i] != '\0'; i++) {
        time_text[position++] = from[i];
    }
    for (int i = 0; address[i] != '\0'; i++) {
        time_text[position++] = address[i];
    }
    const char *sequence = ": icmp_seq=";
    for (int i = 0; sequence[i] != '\0'; i++) {
        time_text[position++] = sequence[i];
    }
    char sequence_buf[8];
    append_uint(sequence_buf, reply->sequence);
    for (int i = 0; sequence_buf[i] != '\0'; i++) {
        time_text[position++] = sequence_buf[i];
    }
    const char *ttl = " ttl=";
    for (int i = 0; ttl[i] != '\0'; i++) {
        time_text[position++] = ttl[i];
    }
    char ttl_buf[8];
    append_uint(ttl_buf, reply->ttl);
    for (int i = 0; ttl_buf[i] != '\0'; i++) {
        time_text[position++] = ttl_buf[i];
    }
    const char *time = " time=";
    for (int i = 0; time[i] != '\0'; i++) {
        time_text[position++] = time[i];
    }
    if (reply->time_ms == 0) {
        const char *fast = "<1ms";
        for (int i = 0; fast[i] != '\0'; i++) {
            time_text[position++] = fast[i];
        }
    } else {
        char ms_buf[16];
        append_uint(ms_buf, reply->time_ms);
        for (int i = 0; ms_buf[i] != '\0'; i++) {
            time_text[position++] = ms_buf[i];
        }
        time_text[position++] = 'm';
        time_text[position++] = 's';
    }
    time_text[position] = '\0';

    ping_line_clear(*cursor_y);
    print(number, WHITE, 0, *cursor_y);
    print(time_text, GREEN, strlen(number), *cursor_y);
    scrl(cursor_y);
}

void cmd_ping(char *args, int *cursor_y) {
    char *arguments = args;
    char *target_text = cmd_next_token(&arguments);
    char *count_text = cmd_next_token(&arguments);
    uint32_t address;
    unsigned int count = 4;

    if (target_text == 0 || cmd_next_token(&arguments) != 0 ||
        !net_parse_ipv4(target_text, &address)) {
        print("Usage: ping <ipv4-address> [count]", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    if (count_text != 0) {
        count = 0;
        for (int i = 0; count_text[i] != '\0'; i++) {
            if (count_text[i] < '0' || count_text[i] > '9') {
                count = 0;
                break;
            }
            count = count * 10u + (unsigned int)(count_text[i] - '0');
        }
        if (count == 0 || count > 10) {
            print("ping: count must be between 1 and 10.", RED, 0, *cursor_y);
            scrl(cursor_y);
            return;
        }
    }

    const net_lease_t *lease = net_get_lease();
    if (lease == 0 || !lease->valid) {
        print("ping: no IP address. Run 'dhcp' first.", RED, 0, *cursor_y);
        scrl(cursor_y);
        return;
    }

    char target[16];
    net_format_ip(address, target);

    ping_line_clear(*cursor_y);
    print("PING ", WHITE, 0, *cursor_y);
    print(target, GREEN, 5, *cursor_y);
    print(" (", WHITE, 5 + strlen(target), *cursor_y);
    print(target, LIGHT_GREY, 7 + strlen(target), *cursor_y);
    print("): 32 data bytes", WHITE, 7 + strlen(target) * 2, *cursor_y);
    scrl(cursor_y);

    unsigned int received = 0;
    for (unsigned int sequence = 1; sequence <= count; sequence++) {
        net_ping_reply_t reply;
        int result = net_ping(address, (uint16_t)sequence, 2, &reply);

        if (result == 1) {
            print_ping_reply(&reply, cursor_y);
            received++;
        } else if (result == NET_ERR_NO_LEASE) {
            print("ping: no IP address. Run 'dhcp' first.", RED, 0, *cursor_y);
            scrl(cursor_y);
            break;
        } else if (result == NET_ERR_NO_ROUTE) {
            print("ping: no gateway for this destination.", RED, 0, *cursor_y);
            scrl(cursor_y);
            break;
        } else if (result == NET_ERR_NO_DEVICE) {
            print("ping: no supported network device.", RED, 0, *cursor_y);
            scrl(cursor_y);
            break;
        } else {
            char seq_buf[8];
            append_uint(seq_buf, sequence);
            ping_line_clear(*cursor_y);
            print("Request timeout for icmp_seq=", RED, 0, *cursor_y);
            print(seq_buf, WHITE, 30, *cursor_y);
            scrl(cursor_y);
        }
    }

    char received_buf[16];
    append_uint(received_buf, received);
    char count_buf[16];
    append_uint(count_buf, count);
    unsigned int loss = 0;
    if (count > 0) {
        loss = ((count - received) * 100u) / count;
    }
    char loss_buf[16];
    append_uint(loss_buf, loss);

    print("--- ", LIGHT_GREY, 0, *cursor_y);
    print(target, LIGHT_GREY, 4, *cursor_y);
    print(" ping statistics ---", LIGHT_GREY, 4 + strlen(target), *cursor_y);
    scrl(cursor_y);

    ping_line_clear(*cursor_y);
    print(count_buf, WHITE, 0, *cursor_y);
    print(" transmitted, ", WHITE, strlen(count_buf), *cursor_y);
    print(received_buf, GREEN, strlen(count_buf) + 14, *cursor_y);
    print(" received, ", WHITE, strlen(count_buf) + 14 + strlen(received_buf), *cursor_y);
    print(loss_buf, loss == 0 ? GREEN : RED,
          strlen(count_buf) + 25 + strlen(received_buf), *cursor_y);
    print("% packet loss", WHITE,
          strlen(count_buf) + 25 + strlen(received_buf) + strlen(loss_buf), *cursor_y);
    scrl(cursor_y);
}
