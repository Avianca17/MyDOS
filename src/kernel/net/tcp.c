#include <mydos.h>
#include <net/net.h>
#include <net/net_checksum.h>
#include <net/net_timer.h>
#include <net/tcp.h>

#define TCP_SYN 0x02u
#define TCP_RST 0x04u
#define TCP_PSH 0x08u
#define TCP_ACK 0x10u
#define TCP_FIN 0x01u
#define TCP_HEADER_SIZE 20u
#define TCP_WINDOW 8192u
#define TCP_CONNECT_TIMEOUT_MS 1000u
#define TCP_RECV_TIMEOUT_MS 8000u

typedef struct __attribute__((packed)) {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t type;
} tcp_ethernet_t;

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
} tcp_ipv4_t;

typedef struct __attribute__((packed)) {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence;
    uint32_t acknowledgment;
    uint8_t data_offset_flags;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} tcp_header_t;

typedef struct {
    uint8_t *payload;
    uint32_t payload_length;
    uint8_t flags;
    uint32_t sequence;
    uint32_t acknowledgment;
} tcp_incoming_t;

static uint8_t tcp_frame[NET_MAX_FRAME];
static uint8_t tcp_receive_frame[NET_MAX_FRAME];
static uint8_t tcp_checksum_block[12 + NET_MAX_FRAME];
static uint16_t tcp_ip_id;

static uint16_t tcp_be16(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint32_t tcp_be32(uint32_t value) {
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8) |
           ((value & 0x00FF0000u) >> 8) |
           ((value & 0xFF000000u) >> 24);
}

static uint16_t tcp_read16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t tcp_read32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}

static void tcp_write32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static uint16_t tcp_segment_checksum(uint32_t source, uint32_t destination,
                                     const uint8_t *segment, uint32_t length) {
    memcpy(tcp_checksum_block, &source, 4);
    memcpy(tcp_checksum_block + 4, &destination, 4);
    tcp_checksum_block[8] = 0;
    tcp_checksum_block[9] = 6;
    tcp_checksum_block[10] = (uint8_t)(length >> 8);
    tcp_checksum_block[11] = (uint8_t)length;
    memcpy(tcp_checksum_block + 12, segment, length);
    return net_checksum16(tcp_checksum_block, 12 + length);
}

static uint16_t tcp_build_frame(tcp_socket_t *socket, uint8_t flags,
                                const uint8_t *payload, uint32_t payload_length) {
    const net_device_t *device = net_device();
    if (device == 0) {
        return 0;
    }

    uint32_t segment_length = TCP_HEADER_SIZE + payload_length;
    uint32_t frame_length = 14 + 20 + segment_length;
    if (frame_length > NET_MAX_FRAME) {
        return 0;
    }

    memset(tcp_frame, 0, frame_length);
    tcp_ethernet_t *ethernet = (tcp_ethernet_t *)tcp_frame;
    tcp_ipv4_t *ip = (tcp_ipv4_t *)(tcp_frame + 14);
    tcp_header_t *tcp = (tcp_header_t *)(tcp_frame + 34);

    memcpy(ethernet->destination, socket->next_hop_mac, 6);
    memcpy(ethernet->source, device->mac, 6);
    ethernet->type = tcp_be16(0x0800u);

    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = tcp_be16((uint16_t)(20 + segment_length));
    ip->identification = tcp_be16(++tcp_ip_id);
    ip->fragment_offset = 0;
    ip->ttl = 64;
    ip->protocol = 6;
    ip->checksum = 0;
    memcpy(ip->source, &socket->local_ip, 4);
    memcpy(ip->destination, &socket->server_ip, 4);
    ip->checksum = net_checksum16((const uint8_t *)ip, 20);

    tcp->source_port = tcp_be16(socket->local_port);
    tcp->destination_port = tcp_be16(socket->server_port);
    tcp_write32((uint8_t *)&tcp->sequence, socket->sequence);
    tcp_write32((uint8_t *)&tcp->acknowledgment, socket->acknowledgment);
    tcp->data_offset_flags = (uint8_t)((TCP_HEADER_SIZE / 4u) << 4);
    tcp->flags = flags;
    tcp->window = tcp_be16(TCP_WINDOW);
    tcp->checksum = 0;
    tcp->urgent = 0;
    if (payload_length != 0 && payload != 0) {
        memcpy(tcp_frame + 54, payload, payload_length);
    }
    tcp->checksum = tcp_be16(tcp_segment_checksum(socket->local_ip,
                                                  socket->server_ip,
                                                  tcp_frame + 34,
                                                  segment_length));
    return (uint16_t)frame_length;
}

static int tcp_parse_frame(const uint8_t *frame, int length,
                           const tcp_socket_t *socket, tcp_incoming_t *incoming) {
    if (length < 54 || tcp_be16(tcp_read16(frame + 12)) != 0x0800u) {
        return 0;
    }
    tcp_ipv4_t *ip = (tcp_ipv4_t *)(frame + 14);
    uint32_t source;
    memcpy(&source, ip->source, 4);
    if (ip->protocol != 6 || source != socket->server_ip) {
        return 0;
    }
    uint16_t ip_length = tcp_read16((const uint8_t *)&ip->total_length);
    if (ip_length < 40 || ip_length > (uint16_t)(length - 14)) {
        return 0;
    }

    tcp_header_t *tcp = (tcp_header_t *)(frame + 34);
    if (tcp_read16((const uint8_t *)&tcp->destination_port) != socket->local_port ||
        tcp_read16((const uint8_t *)&tcp->source_port) != socket->server_port) {
        return 0;
    }
    uint8_t data_offset = (uint8_t)((tcp->data_offset_flags >> 4) & 0x0Fu);
    if (data_offset < 5) {
        return 0;
    }
    uint32_t tcp_header_length = data_offset * 4u;
    uint32_t segment_length = ip_length - 20u;
    if (tcp_header_length > segment_length || 34u + segment_length > (uint32_t)length) {
        return 0;
    }

    uint16_t checksum = tcp_be16(tcp_read16((const uint8_t *)&tcp->checksum));
    if (checksum != 0 && tcp_segment_checksum(source, socket->local_ip,
                                              frame + 34, segment_length) != 0) {
        return 0;
    }

    incoming->flags = tcp->flags;
    incoming->sequence = tcp_be32(tcp_read32((const uint8_t *)&tcp->sequence));
    incoming->acknowledgment = tcp_be32(tcp_read32((const uint8_t *)&tcp->acknowledgment));
    incoming->payload = (uint8_t *)(frame + 34 + tcp_header_length);
    incoming->payload_length = segment_length - tcp_header_length;
    return 1;
}

static int tcp_resolve_route(tcp_socket_t *socket, uint32_t server_ip) {
    const net_lease_t *lease = net_get_lease();
    if (lease == 0 || !lease->valid) {
        if (net_dhcp(0) <= 0) {
            return 0;
        }
        lease = net_get_lease();
    }
    if (lease == 0 || !lease->valid || net_device() == 0) {
        return 0;
    }

    uint32_t own = lease->ip;
    uint32_t next_hop = ((server_ip & lease->netmask) == (own & lease->netmask))
        ? server_ip
        : lease->gateway;
    if (next_hop == 0 || !net_arp_resolve(next_hop, socket->next_hop_mac, 3)) {
        return 0;
    }

    socket->local_ip = own;
    socket->server_ip = server_ip;
    return 1;
}

int tcp_connect(tcp_socket_t *socket, uint32_t server_ip, uint16_t server_port) {
    if (socket == 0) {
        return 0;
    }
    memset(socket, 0, sizeof(*socket));
    if (!tcp_resolve_route(socket, server_ip)) {
        return 0;
    }

    uint32_t ticks;
    __asm__ volatile ("rdtsc" : "=a"(ticks) : : "edx");
    socket->local_port = (uint16_t)(0x4000u + (ticks & 0x3FFFu));
    socket->server_port = server_port;
    socket->sequence = ticks;
    socket->acknowledgment = 0;
    socket->connected = 0;

    for (int attempt = 0; attempt < 4; attempt++) {
        uint16_t length = tcp_build_frame(socket, TCP_SYN, 0, 0);
        if (length == 0 || !net_send(tcp_frame, length)) {
            return 0;
        }

        net_timer_t timer;
        net_timer_begin(&timer);
        while (net_timer_elapsed_ms(&timer) < TCP_CONNECT_TIMEOUT_MS) {
            int received = net_receive(tcp_receive_frame, sizeof(tcp_receive_frame));
            if (received <= 0) {
                continue;
            }
            tcp_incoming_t incoming;
            if (!tcp_parse_frame(tcp_receive_frame, received, socket, &incoming)) {
                continue;
            }
            if ((incoming.flags & TCP_RST) != 0) {
                net_timer_end();
                return 0;
            }
            if ((incoming.flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                socket->acknowledgment = incoming.sequence + 1;
                socket->sequence++;
                length = tcp_build_frame(socket, TCP_ACK, 0, 0);
                net_timer_end();
                if (length == 0 || !net_send(tcp_frame, length)) {
                    return 0;
                }
                socket->connected = 1;
                return 1;
            }
        }
        net_timer_end();
    }
    return 0;
}

int tcp_send(tcp_socket_t *socket, const uint8_t *data, uint32_t length) {
    if (socket == 0 || !socket->connected) {
        return 0;
    }
    uint16_t frame_length = tcp_build_frame(socket, TCP_PSH | TCP_ACK, data, length);
    if (frame_length == 0 || !net_send(tcp_frame, frame_length)) {
        return 0;
    }
    socket->sequence += length;
    return 1;
}

int tcp_recv(tcp_socket_t *socket, uint8_t *buffer, uint32_t capacity,
             uint32_t timeout_ms, uint32_t *received) {
    if (socket == 0 || buffer == 0 || received == 0 || !socket->connected) {
        return -1;
    }
    *received = 0;

    net_timer_t timer;
    net_timer_begin(&timer);
    while (net_timer_elapsed_ms(&timer) < timeout_ms) {
        int length = net_receive(tcp_receive_frame, sizeof(tcp_receive_frame));
        if (length <= 0) {
            continue;
        }
        tcp_incoming_t incoming;
        if (!tcp_parse_frame(tcp_receive_frame, length, socket, &incoming)) {
            continue;
        }
        if ((incoming.flags & TCP_RST) != 0) {
            socket->connected = 0;
            net_timer_end();
            return 0;
        }
        if (incoming.payload_length != 0) {
            uint32_t copy_length = incoming.payload_length;
            if (copy_length > capacity) {
                copy_length = capacity;
            }
            memcpy(buffer, incoming.payload, copy_length);
            *received = copy_length;
            socket->acknowledgment = incoming.sequence + incoming.payload_length;
            uint16_t frame_length = tcp_build_frame(socket, TCP_ACK, 0, 0);
            if (frame_length != 0) {
                net_send(tcp_frame, frame_length);
            }
            net_timer_end();
            return 1;
        }
        if ((incoming.flags & TCP_FIN) != 0) {
            socket->acknowledgment = incoming.sequence + 1;
            uint16_t frame_length = tcp_build_frame(socket, TCP_ACK, 0, 0);
            if (frame_length != 0) {
                net_send(tcp_frame, frame_length);
            }
            socket->connected = 0;
            net_timer_end();
            return 0;
        }
    }
    net_timer_end();
    return -1;
}

void tcp_close(tcp_socket_t *socket) {
    if (socket == 0 || !socket->connected) {
        return;
    }
    uint16_t length = tcp_build_frame(socket, TCP_FIN | TCP_ACK, 0, 0);
    if (length != 0) {
        net_send(tcp_frame, length);
        socket->sequence++;
    }
    socket->connected = 0;
}
