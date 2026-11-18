#include <mydos.h>
#include <net/net.h>

static void clear_line(int y) {
    print("                                                                      ",
          WHITE, 0, y);
}

static void format_ip(uint32_t address, char *out) {
    const uint8_t *bytes = (const uint8_t *)&address;
    int position = 0;

    for (unsigned int i = 0; i < 4; i++) {
        char number[4];
        append_uint(number, bytes[i]);
        for (unsigned int j = 0; number[j] != '\0'; j++) {
            out[position++] = number[j];
        }
        if (i != 3) {
            out[position++] = '.';
        }
    }

    out[position] = '\0';
}

static void format_mac(const uint8_t *mac, char *out) {
    static const char hex[] = "0123456789ABCDEF";
    int position = 0;

    for (unsigned int i = 0; i < 6; i++) {
        out[position++] = hex[mac[i] >> 4];
        out[position++] = hex[mac[i] & 0x0Fu];
        if (i != 5) {
            out[position++] = ':';
        }
    }

    out[position] = '\0';
}

static void print_ip_line(const char *label, uint32_t address, int *cursor_y) {
    char text[16];
    format_ip(address, text);
    clear_line(*cursor_y);
    print(label, WHITE, 0, *cursor_y);
    print(text, GREEN, 15, *cursor_y);
    scrl(cursor_y);
}

static void print_lease(const net_lease_t *lease, int *cursor_y) {
    print_ip_line("  IP address : ", lease->ip, cursor_y);
    print_ip_line("  Netmask    : ", lease->netmask, cursor_y);
    print_ip_line("  Gateway    : ", lease->gateway, cursor_y);
    print_ip_line("  DNS        : ", lease->dns, cursor_y);
    print_ip_line("  Server     : ", lease->server, cursor_y);

    char lease_time[16];
    append_uint(lease_time, lease->lease_seconds);
    clear_line(*cursor_y);
    print("  Lease      : ", WHITE, 0, *cursor_y);
    print(lease_time, GREEN, 15, *cursor_y);
    print(" seconds", WHITE, 15 + strlen(lease_time), *cursor_y);
    scrl(cursor_y);
}

void net_command(char *args, int *cursor_y) {
    int y = *cursor_y;

    if (strcmp(args, "status") == 0) {
        const net_lease_t *lease = net_get_lease();
        if (lease == 0 || !lease->valid) {
            print("dhcp: no lease. Run 'dhcp' first.", RED, 0, y);
            scrl(&y);
        } else {
            print("DHCP lease", YELLOW, 0, y);
            scrl(&y);
            print_lease(lease, &y);
        }
        *cursor_y = y;
        return;
    }

    if (args[0] != '\0' && strcmp(args, "renew") != 0 &&
        strcmp(args, "-r") != 0) {
        print("Usage: dhcp [renew|status]", RED, 0, y);
        scrl(&y);
        *cursor_y = y;
        return;
    }

    print("dhcp: scanning for a supported Ethernet controller...", WHITE, 0, y);
    scrl(&y);

    if (!net_init()) {
        print("dhcp: no supported NIC found (Intel e1000 or Realtek RTL8139).",
              RED, 0, y);
        scrl(&y);
        *cursor_y = y;
        return;
    }

    const net_device_t *device = net_device();
    clear_line(y);
    print("  Device     : ", WHITE, 0, y);
    print(device->name, GREEN, 15, y);
    scrl(&y);

    char mac[18];
    format_mac(device->mac, mac);
    clear_line(y);
    print("  MAC        : ", WHITE, 0, y);
    print(mac, GREEN, 15, y);
    scrl(&y);

    print("dhcp: sending discover...", LIGHT_GREY, 0, y);
    scrl(&y);

    net_lease_t lease;
    int result = net_dhcp(&lease);
    if (result == 1) {
        print("dhcp: lease acquired.", GREEN, 0, y);
        scrl(&y);
        print_lease(&lease, &y);
    } else if (result == NET_ERR_NO_DEVICE) {
        print("dhcp: no supported network device.", RED, 0, y);
        scrl(&y);
    } else {
        print("dhcp: no response from a DHCP server (timeout).", RED, 0, y);
        scrl(&y);
    }

    *cursor_y = y;
}
