#include "kernel.h"
#include "hardware.h"

void cpuid_vendor(char out[13]) {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    __asm__ volatile (
        "cpuid"
        : "=a"(eax),
          "=b"(ebx),
          "=c"(ecx),
          "=d"(edx)
        : "a"(0)
    );

    out[0] = (char)(ebx & 0xFF);
    out[1] = (char)((ebx >> 8) & 0xFF);
    out[2] = (char)((ebx >> 16) & 0xFF);
    out[3] = (char)((ebx >> 24) & 0xFF);

    out[4] = (char)(edx & 0xFF);
    out[5] = (char)((edx >> 8) & 0xFF);
    out[6] = (char)((edx >> 16) & 0xFF);
    out[7] = (char)((edx >> 24) & 0xFF);

    out[8] = (char)(ecx & 0xFF);
    out[9] = (char)((ecx >> 8) & 0xFF);
    out[10] = (char)((ecx >> 16) & 0xFF);
    out[11] = (char)((ecx >> 24) & 0xFF);

    out[12] = '\0';
}

unsigned int get_total_ram_mb(unsigned int mbi) {
    unsigned int* info = (unsigned int*)mbi;
    unsigned int flags = info[0];

    if ((flags & 0x1u) != 0u) {
        unsigned int total_kb = info[1] + info[2];
        return total_kb / 1024u;
    }

    if ((flags & 0x40u) == 0u) return 0;

    unsigned int mmap_addr = info[12];
    unsigned int mmap_length = info[11];
    unsigned int total = 0;
    unsigned int offset = 0;

    while (offset < mmap_length) {
        struct mmap_entry {
            unsigned int size;
            unsigned int addr_low;
            unsigned int addr_high;
            unsigned int len_low;
            unsigned int len_high;
            unsigned int type;
        } __attribute__((packed)) *entry = (struct mmap_entry*)(mmap_addr + offset);

        if (entry->type == 1u) {
            total += ((entry->len_low + 1023u) / 1024u) / 1024u;
        }

        offset += entry->size + sizeof(entry->size);
    }

    return total;
}
