#include <mydos.h>
#include <net/net.h>

#define ETHERTYPE_IPV4 0x0800u
#define IP_PROTOCOL_ICMP 1u
#define ICMP_ECHO_REQUEST 8u
#define ICMP_ECHO_REPLY 0u

#define PIT_CHANNEL_2 0x42
#define PIT_COMMAND 0x43
#define SPEAKER_CONTROL 0x61
#define PING_TIMER_DIVISOR 11932u

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
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} icmp_header_t;

typedef struct {
    uint16_t start;
    uint16_t last;
    uint32_t wraps;
} ping_timer_t;

static uint8_t ping_frame[NET_MAX_FRAME];
static uint8_t ping_receive[NET_MAX_FRAME];
static uint16_t ping_identifier;

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint16_t byte_swap16(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
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

static unsigned int elapsed_seconds(unsigned int start) {
    unsigned int now = bcdtb(rtc_read(0x00));
    if (now >= start) {
        return now - start;
    }
    return 60u - start + now;
}

static uint16_t ping_timer_read(void) {
    outb(PIT_COMMAND, 0x80);
    uint8_t low = io_inb(PIT_CHANNEL_2);
    uint8_t high = io_inb(PIT_CHANNEL_2);
    return (uint16_t)(((uint16_t)high << 8) | low);
}

static void ping_timer_begin(ping_timer_t *timer) {
    uint8_t control = io_inb(SPEAKER_CONTROL);
    outb(SPEAKER_CONTROL, (uint8_t)((control & ~0x02u) | 0x01u));
    outb(PIT_COMMAND, 0xB4);
    outb(PIT_CHANNEL_2, (uint8_t)(PING_TIMER_DIVISOR & 0xFFu));
    outb(PIT_CHANNEL_2, (uint8_t)(PING_TIMER_DIVISOR >> 8));

    timer->start = ping_timer_read();
    timer->last = timer->start;
    timer->wraps = 0;
}

static uint32_t ping_timer_elapsed_ticks(ping_timer_t *timer) {
    uint16_t now = ping_timer_read();

    if (now > timer->last) {
        timer->wraps++;
    }
    timer->last = now;

    return timer->wraps * PING_TIMER_DIVISOR +
           (uint32_t)((int)timer->start - (int)now);
}

static uint32_t ping_timer_elapsed_ms(ping_timer_t *timer) {
    uint32_t ticks = ping_timer_elapsed_ticks(timer);
    return (ticks * 10u) / PING_TIMER_DIVISOR;
}

static void ping_timer_stop(void) {
    uint8_t control = io_inb(SPEAKER_CONTROL);
    outb(SPEAKER_CONTROL, (uint8_t)(control & ~0x03u));
}

static int ping_send(uint32_t source, uint32_t destination,
                     const uint8_t destination_mac[6],
                     const net_device_t *device, uint16_t sequence) {
    memset(ping_frame, 0, sizeof(ping_frame));

    ethernet_header_t *ethernet = (ethernet_header_t *)(void *)ping_frame;
    ipv4_header_t *ip =
        (ipv4_header_t *)(void *)(ping_frame + sizeof(ethernet_header_t));
    icmp_header_t *icmp =
        (icmp_header_t *)(void *)((uint8_t *)ip + sizeof(ipv4_header_t));
    uint8_t *payload = (uint8_t *)icmp + sizeof(icmp_header_t);
    uint16_t payload_length = 32;
    uint16_t icmp_length = (uint16_t)(sizeof(icmp_header_t) + payload_length);
    uint16_t ip_length = (uint16_t)(sizeof(ipv4_header_t) + icmp_length);

    memcpy(ethernet->destination, destination_mac, 6);
    memcpy(ethernet->source, device->mac, 6);
    ethernet->type = byte_swap16(ETHERTYPE_IPV4);

    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = byte_swap16(ip_length);
    ip->identification = byte_swap16(++ping_identifier);
    ip->fragment_offset = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTOCOL_ICMP;
    ip->checksum = 0;
    memcpy(&ip->source, &source, 4);
    memcpy(&ip->destination, &destination, 4);
    ip->checksum = byte_swap16(checksum16((const uint8_t *)ip,
                                          sizeof(ipv4_header_t)));

    for (uint16_t i = 0; i < payload_length; i++) {
        payload[i] = (uint8_t)('a' + (i % 26));
    }

    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->identifier = byte_swap16(0x4D44u);
    icmp->sequence = byte_swap16(sequence);
    icmp->checksum = byte_swap16(checksum16((const uint8_t *)icmp,
                                            icmp_length));

    return net_send(ping_frame,
                    (uint16_t)(sizeof(ethernet_header_t) + ip_length));
}

static int ping_parse(const uint8_t *frame, int length, uint32_t target,
                      const net_lease_t *lease, uint16_t sequence,
                      net_ping_reply_t *reply) {
    if (length < (int)(sizeof(ethernet_header_t) + sizeof(ipv4_header_t) +
                       sizeof(icmp_header_t))) {
        return 0;
    }

    const ethernet_header_t *ethernet =
        (const ethernet_header_t *)(const void *)frame;
    if (ethernet->type != byte_swap16(ETHERTYPE_IPV4)) {
        return 0;
    }

    const ipv4_header_t *ip =
        (const ipv4_header_t *)(const void *)(frame + sizeof(*ethernet));
    if ((ip->version_ihl >> 4) != 4 || ip->protocol != IP_PROTOCOL_ICMP ||
        ip->source != target || ip->destination != lease->ip) {
        return 0;
    }

    uint8_t ip_header_length = (uint8_t)((ip->version_ihl & 0x0Fu) * 4u);
    uint16_t ip_total_length = byte_swap16(ip->total_length);
    if (ip_header_length < sizeof(ipv4_header_t) ||
        ip_total_length < ip_header_length + sizeof(icmp_header_t) ||
        (int)(sizeof(*ethernet) + ip_total_length) > length) {
        return 0;
    }

    const icmp_header_t *icmp =
        (const icmp_header_t *)(const void *)((const uint8_t *)ip +
                                              ip_header_length);
    if (icmp->type != ICMP_ECHO_REPLY || icmp->code != 0 ||
        icmp->identifier != byte_swap16(0x4D44u) ||
        icmp->sequence != byte_swap16(sequence)) {
        return 0;
    }

    if (reply != 0) {
        reply->address = ip->source;
        reply->sequence = sequence;
        reply->bytes = (uint16_t)(ip_total_length - ip_header_length);
        reply->ttl = ip->ttl;
        reply->time_ms = 0;
    }

    return 1;
}

int net_ping(uint32_t address, uint16_t sequence,
             unsigned int timeout_seconds, net_ping_reply_t *reply) {
    const net_lease_t *lease = net_get_lease();

    if (address == 0) {
        return NET_ERR_INVALID;
    }
    if (lease == 0 || !lease->valid) {
        return NET_ERR_NO_LEASE;
    }
    if (!net_init()) {
        return NET_ERR_NO_DEVICE;
    }

    const net_device_t *device = net_device();
    uint32_t destination = address;
    uint32_t next_hop = address;
    uint8_t destination_mac[6];

    if ((address & lease->netmask) != (lease->ip & lease->netmask)) {
        next_hop = lease->gateway;
        if (next_hop == 0) {
            return NET_ERR_NO_ROUTE;
        }
    }

    if (next_hop == lease->ip) {
        memcpy(destination_mac, device->mac, 6);
    } else {
        int arp_result = net_arp_resolve(next_hop, destination_mac, timeout_seconds);
        if (arp_result != 1) {
            return arp_result;
        }
    }

    ping_timer_t timer;
    ping_timer_begin(&timer);

    if (!ping_send(lease->ip, destination, destination_mac, device, sequence)) {
        ping_timer_stop();
        return NET_ERR_INVALID;
    }

    unsigned int start = bcdtb(rtc_read(0x00));
    for (;;) {
        int length = net_receive(ping_receive, sizeof(ping_receive));
        if (length > 0 &&
            ping_parse(ping_receive, length, address, lease, sequence, reply)) {
            if (reply != 0) {
                reply->time_ms = ping_timer_elapsed_ms(&timer);
            }
            ping_timer_stop();
            return 1;
        }

        ping_timer_elapsed_ticks(&timer);

        if (elapsed_seconds(start) >= timeout_seconds) {
            ping_timer_stop();
            return NET_ERR_TIMEOUT;
        }
    }
}
