#ifndef MYDOS_NET_CHECKSUM_H
#define MYDOS_NET_CHECKSUM_H

#include <stdint.h>

uint16_t net_checksum16(const uint8_t *data, uint32_t length);

#endif
