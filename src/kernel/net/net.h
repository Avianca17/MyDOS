#ifndef MYDOS_NET_H
#define MYDOS_NET_H

#include <stdint.h>

#define NET_MAX_FRAME 1600
#define NET_ERR_NO_DEVICE -1
#define NET_ERR_TIMEOUT -2
#define NET_ERR_INVALID -3
#define NET_ERR_NO_LEASE -4
#define NET_ERR_NO_ROUTE -5

typedef struct net_device {
    const char *name;
    uint8_t mac[6];
    int (*send)(const uint8_t *frame, uint16_t length);
    int (*receive)(uint8_t *frame, uint16_t capacity);
} net_device_t;

typedef struct net_lease {
    int valid;
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint32_t server;
    uint32_t lease_seconds;
} net_lease_t;

typedef struct net_ping_reply {
    uint32_t address;
    uint16_t sequence;
    uint16_t bytes;
    uint8_t ttl;
    uint32_t time_ms;
} net_ping_reply_t;

int net_init(void);
int net_send(const void *frame, uint16_t length);
int net_receive(void *frame, uint16_t capacity);
const net_device_t *net_device(void);
const net_lease_t *net_get_lease(void);
int net_dhcp(net_lease_t *lease);
int net_arp_resolve(uint32_t address, uint8_t mac[6],
                    unsigned int timeout_seconds);
int net_ping(uint32_t address, uint16_t sequence,
             unsigned int timeout_seconds, net_ping_reply_t *reply);
void net_format_ip(uint32_t address, char *out);
int net_parse_ipv4(const char *text, uint32_t *address);

int e1000_init(net_device_t *device);
int rtl8139_init(net_device_t *device);

#endif
