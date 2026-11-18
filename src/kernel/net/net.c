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
