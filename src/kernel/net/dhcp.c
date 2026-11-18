#include <mydos.h>
#include <net/net.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCP_MESSAGE_SIZE 300
#define DHCP_OPTIONS_SIZE 312
#define DHCP_COOKIE 0x63825363u

#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPACK 5
#define DHCPNAK 6

typedef struct __attribute__((packed)) {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t type;
} ethernet_header_t;

typedef struct __attribute__((packed)) {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t fragment_offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t source;
    uint32_t destination;
} ipv4_header_t;

typedef struct __attribute__((packed)) {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;

typedef struct __attribute__((packed)) {
    uint8_t op;
    uint8_t hardware_type;
    uint8_t hardware_length;
    uint8_t hops;
    uint32_t transaction_id;
    uint16_t seconds;
    uint16_t flags;
    uint32_t client_address;
    uint32_t your_address;
    uint32_t server_address;
    uint32_t gateway_address;
    uint8_t client_hardware[16];
    uint8_t server_name[64];
    uint8_t boot_file[128];
    uint32_t cookie;
    uint8_t options[DHCP_OPTIONS_SIZE];
} dhcp_packet_t;

typedef struct {
    uint32_t address;
    uint32_t server;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint32_t lease_seconds;
} dhcp_reply_t;

static uint8_t dhcp_frame[NET_MAX_FRAME];
static uint8_t receive_frame[NET_MAX_FRAME];
static uint16_t ip_identifier;
static net_lease_t dhcp_lease;

static uint16_t byte_swap16(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint32_t byte_swap32(uint32_t value) {
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8) |
           ((value & 0x00FF0000u) >> 8) |
           ((value & 0xFF000000u) >> 24);
}

static uint16_t checksum16(const uint8_t *data, uint16_t length) {
    uint32_t sum = 0;

    while (length > 1) {
        sum += ((uint16_t)data[0] << 8) | data[1];
        data += 2;
        length -= 2;
    }

    if (length != 0) {
        sum += (uint16_t)data[0] << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

static int option_add(uint8_t *options, int *position, uint8_t code,
                      const void *data, uint8_t length) {
    if (*position + 2 + length > DHCP_OPTIONS_SIZE) {
        return 0;
    }

    options[(*position)++] = code;
    options[(*position)++] = length;
    const uint8_t *bytes = (const uint8_t *)data;
    for (uint8_t i = 0; i < length; i++) {
        options[(*position)++] = bytes[i];
    }
    return 1;
}

static int option_add_u8(uint8_t *options, int *position, uint8_t code,
                         uint8_t value) {
    return option_add(options, position, code, &value, 1);
}

static int option_add_u32(uint8_t *options, int *position, uint8_t code,
                          uint32_t value) {
    return option_add(options, position, code, &value, 4);
}

static int dhcp_build_options(uint8_t *options, uint8_t message_type,
                              uint32_t requested_address,
                              uint32_t server_identifier) {
    static const uint8_t parameter_list[] = {
        1, 3, 6, 12, 15, 28, 51, 54, 58, 59
    };
    static const char hostname[] = "mydos";
    const net_device_t *device = net_device();
    int position = 0;

    if (device == 0) {
        return 0;
    }

    if (!option_add_u8(options, &position, 53, message_type)) {
        return 0;
    }

    if (!option_add(options, &position, 55, parameter_list,
                    sizeof(parameter_list))) {
        return 0;
    }

    uint8_t client_id[7];
    client_id[0] = 1;
    memcpy(client_id + 1, device->mac, 6);
    if (!option_add(options, &position, 61, client_id, sizeof(client_id))) {
        return 0;
    }

    if (!option_add(options, &position, 12, hostname,
                    (uint8_t)(sizeof(hostname) - 1))) {
        return 0;
    }

    uint16_t maximum_size = byte_swap16(1500);
    if (!option_add(options, &position, 57, &maximum_size,
                    sizeof(maximum_size))) {
        return 0;
    }

    if (message_type == DHCPREQUEST) {
        if (!option_add_u32(options, &position, 50, requested_address)) {
            return 0;
        }
        if (!option_add_u32(options, &position, 54, server_identifier)) {
            return 0;
        }
    }

    if (position + 1 > DHCP_OPTIONS_SIZE) {
        return 0;
    }
    options[position++] = 255;
    return position;
}

static int dhcp_send(uint8_t message_type, uint32_t transaction_id,
                     uint32_t requested_address, uint32_t server_identifier) {
    const net_device_t *device = net_device();
    if (device == 0) {
        return 0;
    }

    memset(dhcp_frame, 0, sizeof(dhcp_frame));

    ethernet_header_t *ethernet = (ethernet_header_t *)(void *)dhcp_frame;
    ipv4_header_t *ip = (ipv4_header_t *)(void *)(dhcp_frame + sizeof(*ethernet));
    udp_header_t *udp = (udp_header_t *)(void *)((uint8_t *)ip + sizeof(*ip));
    dhcp_packet_t *packet = (dhcp_packet_t *)(void *)((uint8_t *)udp + sizeof(*udp));

    memset(ethernet->destination, 0xFF, 6);
    memcpy(ethernet->source, device->mac, 6);
    ethernet->type = byte_swap16(0x0800);

    packet->op = 1;
    packet->hardware_type = 1;
    packet->hardware_length = 6;
    packet->hops = 0;
    packet->transaction_id = byte_swap32(transaction_id);
    packet->seconds = 0;
    packet->flags = byte_swap16(0x8000);
    packet->client_address = 0;
    packet->your_address = 0;
    packet->server_address = 0;
    packet->gateway_address = 0;
    memcpy(packet->client_hardware, device->mac, 6);
    memset(packet->server_name, 0, sizeof(packet->server_name));
    memset(packet->boot_file, 0, sizeof(packet->boot_file));
    packet->cookie = byte_swap32(DHCP_COOKIE);

    int option_length = dhcp_build_options(packet->options, message_type,
                                           requested_address, server_identifier);
    if (option_length == 0) {
        return 0;
    }

    uint16_t dhcp_length = (uint16_t)(240 + option_length);
    if (dhcp_length < DHCP_MESSAGE_SIZE) {
        dhcp_length = DHCP_MESSAGE_SIZE;
    }

    uint16_t udp_length = (uint16_t)(sizeof(*udp) + dhcp_length);
    uint16_t ip_length = (uint16_t)(sizeof(*ip) + udp_length);

    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = byte_swap16(ip_length);
    ip->identification = byte_swap16(++ip_identifier);
    ip->fragment_offset = 0;
    ip->ttl = 64;
    ip->protocol = 17;
    ip->checksum = 0;
    ip->source = 0;
    ip->destination = 0xFFFFFFFFu;
    ip->checksum = byte_swap16(checksum16((const uint8_t *)ip, sizeof(*ip)));

    udp->source_port = byte_swap16(DHCP_CLIENT_PORT);
    udp->destination_port = byte_swap16(DHCP_SERVER_PORT);
    udp->length = byte_swap16(udp_length);
    udp->checksum = 0;

    return net_send(dhcp_frame, (uint16_t)(sizeof(*ethernet) + ip_length));
}

static int dhcp_parse(const uint8_t *frame, int length, uint32_t transaction_id,
                      const uint8_t *mac, dhcp_reply_t *reply,
                      uint8_t *message_type) {
    if (length < (int)(sizeof(ethernet_header_t) + sizeof(ipv4_header_t) +
                        sizeof(udp_header_t) + 240)) {
        return 0;
    }

    const ethernet_header_t *ethernet = (const ethernet_header_t *)(const void *)frame;
    if (ethernet->type != byte_swap16(0x0800)) {
        return 0;
    }

    const ipv4_header_t *ip = (const ipv4_header_t *)(const void *)(frame + sizeof(*ethernet));
    if ((ip->version_ihl >> 4) != 4) {
        return 0;
    }

    uint8_t ip_header_length = (uint8_t)((ip->version_ihl & 0x0Fu) * 4u);
    if (ip_header_length < sizeof(*ip)) {
        return 0;
    }

    uint16_t ip_total_length = byte_swap16(ip->total_length);
    if (ip_total_length < ip_header_length + sizeof(udp_header_t) + 240 ||
        (int)(sizeof(*ethernet) + ip_total_length) > length) {
        return 0;
    }

    if (ip->protocol != 17) {
        return 0;
    }

    const udp_header_t *udp =
        (const udp_header_t *)(const void *)((const uint8_t *)ip + ip_header_length);
    if (byte_swap16(udp->source_port) != DHCP_SERVER_PORT ||
        byte_swap16(udp->destination_port) != DHCP_CLIENT_PORT) {
        return 0;
    }

    uint16_t udp_length = byte_swap16(udp->length);
    if (udp_length < sizeof(*udp) + 240 ||
        (int)(sizeof(*ethernet) + ip_header_length + udp_length) > length) {
        return 0;
    }

    const dhcp_packet_t *packet =
        (const dhcp_packet_t *)(const void *)((const uint8_t *)udp + sizeof(*udp));
    uint16_t dhcp_length = (uint16_t)(udp_length - sizeof(*udp));

    if (packet->op != 2 ||
        packet->transaction_id != byte_swap32(transaction_id) ||
        memcmp(packet->client_hardware, mac, 6) != 0 ||
        packet->cookie != byte_swap32(DHCP_COOKIE)) {
        return 0;
    }

    dhcp_reply_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    parsed.address = packet->your_address;
    parsed.server = ip->source;

    uint8_t type = 0;
    const uint8_t *options = packet->options;
    uint16_t options_length = (uint16_t)(dhcp_length - 240);
    if (options_length > sizeof(packet->options)) {
        options_length = sizeof(packet->options);
    }

    uint16_t position = 0;
    while (position < options_length) {
        uint8_t code = options[position++];
        if (code == 0) {
            continue;
        }
        if (code == 255) {
            break;
        }
        if (position >= options_length) {
            break;
        }

        uint8_t option_length = options[position++];
        if ((uint16_t)(position + option_length) > options_length) {
            break;
        }

        if (code == 53 && option_length >= 1) {
            type = options[position];
        } else if (code == 54 && option_length >= 4) {
            memcpy(&parsed.server, options + position, 4);
        } else if (code == 1 && option_length >= 4) {
            memcpy(&parsed.netmask, options + position, 4);
        } else if (code == 3 && option_length >= 4) {
            memcpy(&parsed.gateway, options + position, 4);
        } else if (code == 6 && option_length >= 4) {
            memcpy(&parsed.dns, options + position, 4);
        } else if (code == 51 && option_length >= 4) {
            memcpy(&parsed.lease_seconds, options + position, 4);
            parsed.lease_seconds = byte_swap32(parsed.lease_seconds);
        }

        position = (uint16_t)(position + option_length);
    }

    if (type == 0) {
        return 0;
    }

    *message_type = type;
    *reply = parsed;
    return 1;
}

static int dhcp_wait(uint32_t transaction_id, const uint8_t *mac,
                     unsigned int timeout_seconds, dhcp_reply_t *reply,
                     uint8_t *message_type) {
    uint8_t start = bcdtb(rtc_read(0x00));
    uint32_t spins = 0;

    for (;;) {
        int length = net_receive(receive_frame, sizeof(receive_frame));
        if (length > 0) {
            if (dhcp_parse(receive_frame, length, transaction_id, mac,
                           reply, message_type)) {
                return 1;
            }
        }

        spins++;
        if ((spins & 0x3FFu) == 0) {
            uint8_t now = bcdtb(rtc_read(0x00));
            unsigned int elapsed = (now >= start) ?
                                   (unsigned int)(now - start) :
                                   (unsigned int)(60 - start + now);
            if (elapsed >= timeout_seconds) {
                return 0;
            }
        }
    }
}

static uint32_t dhcp_transaction_id(unsigned int attempt) {
    const net_device_t *device = net_device();
    uint32_t id = 0x4D444F53u ^ (attempt * 0x9E3779B9u);

    if (device != 0) {
        for (unsigned int i = 0; i < 6; i++) {
            id = (id << 5) ^ (id >> 27) ^ device->mac[i];
        }
    }

    id ^= ((uint32_t)bcdtb(rtc_read(0x00)) << 8) |
          (uint32_t)bcdtb(rtc_read(0x02));
    return id;
}

const net_lease_t *net_get_lease(void) {
    return &dhcp_lease;
}

int net_dhcp(net_lease_t *lease) {
    memset(&dhcp_lease, 0, sizeof(dhcp_lease));
    if (lease != 0) {
        memset(lease, 0, sizeof(*lease));
    }

    if (!net_init()) {
        return NET_ERR_NO_DEVICE;
    }

    const net_device_t *device = net_device();
    if (device == 0) {
        return NET_ERR_NO_DEVICE;
    }

    for (unsigned int attempt = 0; attempt < 3; attempt++) {
        uint32_t transaction_id = dhcp_transaction_id(attempt);
        if (!dhcp_send(DHCPDISCOVER, transaction_id, 0, 0)) {
            return NET_ERR_TIMEOUT;
        }

        dhcp_reply_t offer;
        uint8_t type = 0;
        if (!dhcp_wait(transaction_id, device->mac, 5, &offer, &type) ||
            type != DHCPOFFER) {
            continue;
        }

        if (!dhcp_send(DHCPREQUEST, transaction_id, offer.address, offer.server)) {
            return NET_ERR_TIMEOUT;
        }

        dhcp_reply_t acknowledgement;
        type = 0;
        if (!dhcp_wait(transaction_id, device->mac, 5, &acknowledgement, &type)) {
            continue;
        }

        if (type == DHCPNAK) {
            continue;
        }

        if (type != DHCPACK) {
            continue;
        }

        net_lease_t result;
        memset(&result, 0, sizeof(result));
        result.valid = 1;
        result.ip = acknowledgement.address;
        result.server = acknowledgement.server;
        result.netmask = acknowledgement.netmask != 0 ? acknowledgement.netmask : 0x00FFFFFFu;
        result.gateway = acknowledgement.gateway;
        result.dns = acknowledgement.dns;
        result.lease_seconds = acknowledgement.lease_seconds;

        dhcp_lease = result;
        if (lease != 0) {
            *lease = result;
        }
        return 1;
    }

    return NET_ERR_TIMEOUT;
}
