#include <net/net_checksum.h>

uint16_t net_checksum16(const uint8_t *data, uint32_t length) {
    uint32_t sum = 0;
    uint32_t index = 0;

    while (length > 1) {
        sum += (uint32_t)(((uint32_t)data[index] << 8) | data[index + 1]);
        index += 2;
        length -= 2;
    }
    if (length != 0) {
        sum += (uint32_t)data[index] << 8;
    }
    while ((sum & 0xFFFF0000u) != 0) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum & 0xFFFFu);
}
