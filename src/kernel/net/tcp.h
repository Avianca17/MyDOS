#ifndef MYDOS_TCP_H
#define MYDOS_TCP_H

#include <stdint.h>

typedef struct {
    uint8_t next_hop_mac[6];
    uint32_t local_ip;
    uint32_t server_ip;
    uint16_t local_port;
    uint16_t server_port;
    uint32_t sequence;
    uint32_t acknowledgment;
    int connected;
} tcp_socket_t;

int tcp_connect(tcp_socket_t *socket, uint32_t server_ip, uint16_t server_port);
int tcp_send(tcp_socket_t *socket, const uint8_t *data, uint32_t length);
int tcp_recv(tcp_socket_t *socket, uint8_t *buffer, uint32_t capacity,
             uint32_t timeout_ms, uint32_t *received);
void tcp_close(tcp_socket_t *socket);

#endif
