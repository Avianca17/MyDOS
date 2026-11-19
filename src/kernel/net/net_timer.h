#ifndef MYDOS_NET_TIMER_H
#define MYDOS_NET_TIMER_H

#include <stdint.h>

typedef struct {
    uint16_t start;
    uint16_t last;
    uint32_t wraps;
} net_timer_t;

void net_timer_begin(net_timer_t *timer);
uint32_t net_timer_elapsed_ms(net_timer_t *timer);
void net_timer_end(void);

#endif
