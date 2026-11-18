#include <mydos.h>
#include <net/net.h>

static net_device_t active_device;
static int network_ready;
static uint8_t padded_frame[NET_MAX_FRAME];

int net_init(void) {
    if (network_ready) {
        return 1;
    }

    memset(&active_device, 0, sizeof(active_device));
    if (e1000_init(&active_device)) {
        network_ready = 1;
        return 1;
    }

    memset(&active_device, 0, sizeof(active_device));
    if (rtl8139_init(&active_device)) {
        network_ready = 1;
        return 1;
    }

    memset(&active_device, 0, sizeof(active_device));
    return 0;
}

int net_send(const void *frame, uint16_t length) {
    if (!network_ready || active_device.send == 0 || frame == 0 || length == 0) {
        return 0;
    }

    if (length < 60) {
        if (length > NET_MAX_FRAME) {
            return 0;
        }
        memcpy(padded_frame, frame, length);
        memset(padded_frame + length, 0, 60 - length);
        return active_device.send(padded_frame, 60);
    }

    if (length > NET_MAX_FRAME) {
        return 0;
    }

    return active_device.send((const uint8_t *)frame, length);
}

int net_receive(void *frame, uint16_t capacity) {
    if (!network_ready || active_device.receive == 0 || frame == 0 || capacity == 0) {
        return 0;
    }

    return active_device.receive((uint8_t *)frame, capacity);
}

const net_device_t *net_device(void) {
    if (!network_ready) {
        return 0;
    }
    return &active_device;
}

void net_format_ip(uint32_t address, char *out) {
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

int net_parse_ipv4(const char *text, uint32_t *address) {
    uint8_t bytes[4];
    int part = 0;
    unsigned int value = 0;
    int digits = 0;

    if (text == 0 || address == 0) {
        return 0;
    }

    while (*text != '\0') {
        if (*text >= '0' && *text <= '9') {
            value = value * 10u + (unsigned int)(*text - '0');
            if (value > 255u) {
                return 0;
            }
            digits++;
        } else if (*text == '.') {
            if (digits == 0 || part >= 3) {
                return 0;
            }
            bytes[part++] = (uint8_t)value;
            value = 0;
            digits = 0;
        } else {
            return 0;
        }
        text++;
    }

    if (digits == 0 || part != 3) {
        return 0;
    }

    bytes[3] = (uint8_t)value;
    memcpy(address, bytes, sizeof(bytes));
    return 1;
}
