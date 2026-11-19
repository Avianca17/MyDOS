#include <mydos.h>
#include <net/net.h>

#define ETHERTYPE_ARP 0x0806u
#define ETHERTYPE_IPV4 0x0800u
#define ARP_REQUEST 1u
#define ARP_REPLY 2u

typedef struct __attribute__((packed)) {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t type;
} ethernet_header_t;

typedef struct __attribute__((packed)) {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_length;
    uint8_t protocol_length;
    uint16_t operation;
    uint8_t sender_hardware[6];
    uint32_t sender_address;
    uint8_t target_hardware[6];
    uint32_t target_address;
} arp_packet_t;

static uint8_t arp_frame[NET_MAX_FRAME];
static uint8_t arp_receive[NET_MAX_FRAME];

static uint16_t byte_swap16(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

static unsigned int elapsed_seconds(unsigned int start) {
    unsigned int now = bcdtb(rtc_read(0x00));
    if (now >= start) {
        return now - start;
    }
    return 60u - start + now;
}

static int arp_send_request(uint32_t address, const net_device_t *device,
                            const net_lease_t *lease) {
    memset(arp_frame, 0, sizeof(arp_frame));

    ethernet_header_t *ethernet = (ethernet_header_t *)(void *)arp_frame;
    arp_packet_t *arp =
        (arp_packet_t *)(void *)(arp_frame + sizeof(ethernet_header_t));

    memset(ethernet->destination, 0xFF, 6);
    memcpy(ethernet->source, device->mac, 6);
    ethernet->type = byte_swap16(ETHERTYPE_ARP);

    arp->hardware_type = byte_swap16(1);
    arp->protocol_type = byte_swap16(ETHERTYPE_IPV4);
    arp->hardware_length = 6;
    arp->protocol_length = 4;
    arp->operation = byte_swap16(ARP_REQUEST);
    memcpy(arp->sender_hardware, device->mac, 6);
    memcpy(&arp->sender_address, &lease->ip, 4);
    memset(arp->target_hardware, 0, 6);
    memcpy(&arp->target_address, &address, 4);

    return net_send(arp_frame,
                    (uint16_t)(sizeof(ethernet_header_t) + sizeof(arp_packet_t)));
}

static int arp_parse(const uint8_t *frame, int length, uint32_t address,
                     const net_device_t *device, const net_lease_t *lease,
                     uint8_t mac[6]) {
    if (length < (int)(sizeof(ethernet_header_t) + sizeof(arp_packet_t))) {
        return 0;
    }

    const ethernet_header_t *ethernet =
        (const ethernet_header_t *)(const void *)frame;
    if (ethernet->type != byte_swap16(ETHERTYPE_ARP)) {
        return 0;
    }

    const arp_packet_t *arp =
        (const arp_packet_t *)(const void *)(frame + sizeof(*ethernet));
    if (arp->hardware_type != byte_swap16(1) ||
        arp->protocol_type != byte_swap16(ETHERTYPE_IPV4) ||
        arp->hardware_length != 6 ||
        arp->protocol_length != 4 ||
        arp->operation != byte_swap16(ARP_REPLY) ||
        arp->sender_address != address ||
        arp->target_address != lease->ip ||
        memcmp(arp->target_hardware, device->mac, 6) != 0) {
        return 0;
    }

    memcpy(mac, arp->sender_hardware, 6);
    return 1;
}

int net_arp_resolve(uint32_t address, uint8_t mac[6],
                    unsigned int timeout_seconds) {
    const net_lease_t *lease = net_get_lease();
    const net_device_t *device = net_device();

    if (address == 0 || mac == 0) {
        return NET_ERR_INVALID;
    }
    if (lease == 0 || !lease->valid) {
        return NET_ERR_NO_LEASE;
    }
    if (device == 0 || !net_init()) {
        return NET_ERR_NO_DEVICE;
    }

    if (!arp_send_request(address, device, lease)) {
        return NET_ERR_INVALID;
    }

    unsigned int start = bcdtb(rtc_read(0x00));
    for (;;) {
        int length = net_receive(arp_receive, sizeof(arp_receive));
        if (length > 0 &&
            arp_parse(arp_receive, length, address, device, lease, mac)) {
            return 1;
        }

        if (elapsed_seconds(start) >= timeout_seconds) {
            return NET_ERR_TIMEOUT;
        }
    }
}
