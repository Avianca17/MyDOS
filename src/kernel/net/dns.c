#include <mydos.h>
#include <net/net.h>
#include <net/net_checksum.h>
#include <net/net_timer.h>

#define DNS_PORT 53u
#define DNS_LOCAL_PORT 0x3535u
#define DNS_TIMEOUT_MS 4000u
#define DNS_MAX_NAME 256u

typedef struct __attribute__((packed)) {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t type;
} dns_ethernet_t;

typedef struct __attribute__((packed)) {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t fragment_offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint8_t source[4];
    uint8_t destination[4];
} dns_ipv4_t;

typedef struct __attribute__((packed)) {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;
} dns_udp_t;

static uint8_t dns_frame[NET_MAX_FRAME];
static uint8_t dns_receive[NET_MAX_FRAME];
static uint16_t dns_identifier;
static uint16_t dns_ip_id;

static uint16_t dns_be16(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint16_t dns_read16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t dns_read_ip(const uint8_t *bytes) {
    uint32_t value;
    memcpy(&value, bytes, sizeof(value));
    return value;
}

static uint16_t dns_write_name(uint8_t *out, const char *name) {
    uint16_t position = 0;

    while (*name != '\0' && position < DNS_MAX_NAME - 2u) {
        const char *dot = strchr(name, '.');
        uint16_t length = (uint16_t)(dot != 0 ? dot - name : (int)strlen(name));
        if (length == 0 || length > 63) {
            return 0;
        }
        out[position++] = (uint8_t)length;
        for (uint16_t i = 0; i < length; i++) {
            char character = name[i];
            if (character >= 'A' && character <= 'Z') {
                character = (char)(character + ('a' - 'A'));
            }
            out[position++] = (uint8_t)character;
        }
        name += length;
        if (*name == '.') {
            name++;
        }
    }
    out[position++] = 0;
    return position;
}

static uint16_t dns_skip_name(const uint8_t *packet, uint16_t length,
                              uint16_t offset) {
    uint16_t start = offset;
    uint16_t consumed = 0;
    int jumped = 0;

    for (;;) {
        if (offset >= length) {
            return 0;
        }
        uint8_t size = packet[offset];
        if ((size & 0xC0u) == 0xC0u) {
            if (offset + 1 >= length) {
                return 0;
            }
            if (!jumped) {
                consumed = (uint16_t)(offset - start + 2);
            }
            offset = (uint16_t)(((uint16_t)(size & 0x3Fu) << 8) | packet[offset + 1]);
            jumped = 1;
            if (offset >= length) {
                return 0;
            }
            continue;
        }
        if (size == 0) {
            return jumped ? consumed : (uint16_t)(offset - start + 1);
        }
        if (size > 63 || offset + 1u + size > length) {
            return 0;
        }
        offset = (uint16_t)(offset + 1u + size);
    }
}

static int dns_parse_answer(const uint8_t *packet, uint16_t length,
                            uint16_t identifier, uint8_t *address) {
    if (length < 12 || dns_read16(packet) != identifier) {
        return 0;
    }
    uint16_t flags = dns_read16(packet + 2);
    if ((flags & 0x8000u) == 0 || (flags & 0x000Fu) != 0) {
        return 0;
    }

    uint16_t questions = dns_read16(packet + 4);
    uint16_t answers = dns_read16(packet + 6);
    uint16_t offset = 12;

    for (uint16_t i = 0; i < questions; i++) {
        uint16_t skipped = dns_skip_name(packet, length, offset);
        if (skipped == 0 || offset + skipped + 4 > length) {
            return 0;
        }
        offset = (uint16_t)(offset + skipped + 4);
    }

    for (uint16_t i = 0; i < answers; i++) {
        uint16_t skipped = dns_skip_name(packet, length, offset);
        if (skipped == 0 || offset + skipped + 10 > length) {
            return 0;
        }
        offset = (uint16_t)(offset + skipped);
        uint16_t type = dns_read16(packet + offset);
        uint16_t class_code = dns_read16(packet + offset + 2);
        uint16_t data_length = dns_read16(packet + offset + 8);
        offset = (uint16_t)(offset + 10);
        if (offset + data_length > length) {
            return 0;
        }
        if (type == 1 && class_code == 1 && data_length == 4) {
            memcpy(address, packet + offset, 4);
            return 1;
        }
        offset = (uint16_t)(offset + data_length);
    }
    return 0;
}

int net_dns_resolve(const char *hostname, uint32_t *address) {
    if (hostname == 0 || address == 0 || *hostname == '\0') {
        return 0;
    }

    const net_lease_t *lease = net_get_lease();
    if (lease == 0 || !lease->valid) {
        if (net_dhcp(0) <= 0) {
            return 0;
        }
        lease = net_get_lease();
    }
    if (lease == 0 || !lease->valid) {
        return 0;
    }

    uint32_t dns_address;
    if (lease->dns == 0) {
        if (!net_parse_ipv4("8.8.8.8", &dns_address)) {
            return 0;
        }
    } else {
        dns_address = lease->dns;
    }

    const net_device_t *device = net_device();
    if (device == 0) {
        return 0;
    }

    uint32_t own = lease->ip;
    uint32_t mask = lease->netmask;
    uint32_t target = dns_address;
    uint32_t next_hop = ((target & mask) == (own & mask)) ? target : lease->gateway;
    if (next_hop == 0) {
        return 0;
    }

    uint8_t next_mac[6];
    if (!net_arp_resolve(next_hop, next_mac, 3)) {
        return 0;
    }

    memset(dns_frame, 0, sizeof(dns_frame));
    dns_ethernet_t *ethernet = (dns_ethernet_t *)dns_frame;
    dns_ipv4_t *ip = (dns_ipv4_t *)(dns_frame + sizeof(dns_ethernet_t));
    dns_udp_t *udp = (dns_udp_t *)(dns_frame + sizeof(dns_ethernet_t) + sizeof(dns_ipv4_t));
    uint8_t *dns = dns_frame + sizeof(dns_ethernet_t) + sizeof(dns_ipv4_t) + sizeof(dns_udp_t);

    if (dns_identifier == 0) {
        uint32_t ticks;
        __asm__ volatile ("rdtsc" : "=a"(ticks) : : "edx");
        dns_identifier = (uint16_t)(ticks | 0x0100u);
    }

    uint16_t name_length = dns_write_name(dns + 12, hostname);
    if (name_length == 0 || 12 + name_length + 4 > NET_MAX_FRAME - 42) {
        return 0;
    }
    uint16_t dns_length = (uint16_t)(12 + name_length + 4);

    memcpy(ethernet->destination, next_mac, 6);
    memcpy(ethernet->source, device->mac, 6);
    ethernet->type = dns_be16(0x0800u);

    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = dns_be16((uint16_t)(20 + 8 + dns_length));
    ip->identification = dns_be16(++dns_ip_id);
    ip->fragment_offset = 0;
    ip->ttl = 64;
    ip->protocol = 17;
    ip->checksum = 0;
    memcpy(ip->source, &own, 4);
    memcpy(ip->destination, &dns_address, 4);
    ip->checksum = dns_be16(net_checksum16((const uint8_t *)ip, 20));

    udp->source_port = dns_be16(DNS_LOCAL_PORT);
    udp->destination_port = dns_be16(DNS_PORT);
    udp->length = dns_be16((uint16_t)(8 + dns_length));
    udp->checksum = 0;

    dns[0] = (uint8_t)(dns_identifier >> 8);
    dns[1] = (uint8_t)dns_identifier;
    dns[2] = 0x01;
    dns[3] = 0x00;
    dns[4] = 0x00;
    dns[5] = 0x01;
    dns[6] = 0;
    dns[7] = 0;
    dns[8] = 0;
    dns[9] = 0;
    dns[10] = 0;
    dns[11] = 0;
    dns[12 + name_length] = 0x00;
    dns[12 + name_length + 1] = 0x01;
    dns[12 + name_length + 2] = 0x00;
    dns[12 + name_length + 3] = 0x01;

    if (!net_send(dns_frame, (uint16_t)(42 + dns_length))) {
        return 0;
    }

    net_timer_t timer;
    net_timer_begin(&timer);
    while (net_timer_elapsed_ms(&timer) < DNS_TIMEOUT_MS) {
        int received = net_receive(dns_receive, sizeof(dns_receive));
        if (received <= 0) {
            continue;
        }
        if ((int)sizeof(dns_ethernet_t) + 20 + 8 > received) {
            continue;
        }
        dns_ethernet_t *reply_ethernet = (dns_ethernet_t *)dns_receive;
        if (dns_be16(reply_ethernet->type) != 0x0800u) {
            continue;
        }
        dns_ipv4_t *reply_ip = (dns_ipv4_t *)(dns_receive + sizeof(dns_ethernet_t));
        if (reply_ip->protocol != 17 || dns_read_ip(reply_ip->source) != dns_address) {
            continue;
        }
        uint16_t ip_length = dns_be16(reply_ip->total_length);
        if (ip_length < 28 || ip_length > received - 14) {
            continue;
        }
        dns_udp_t *reply_udp = (dns_udp_t *)(dns_receive + 14 + 20);
        if (dns_be16(reply_udp->destination_port) != DNS_LOCAL_PORT) {
            continue;
        }
        uint16_t udp_length = dns_be16(reply_udp->length);
        if (udp_length < 8 || 14 + 20 + udp_length > received) {
            continue;
        }
        uint16_t reply_dns_length = (uint16_t)(udp_length - 8);
        if (dns_parse_answer(dns_receive + 42, reply_dns_length,
                             dns_identifier, (uint8_t *)address)) {
            net_timer_end();
            return 1;
        }
    }

    net_timer_end();
    return 0;
}
