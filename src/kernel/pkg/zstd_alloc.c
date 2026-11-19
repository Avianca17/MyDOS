#include <mydos.h>
#include "zstd_alloc.h"

#define ZSTD_ALLOC_ARENA_SIZE (1024u * 1024u)

static unsigned char zstd_alloc_arena[ZSTD_ALLOC_ARENA_SIZE] __attribute__((aligned(16)));
static unsigned int zstd_alloc_used;

void mydos_zstd_reset(void) {
    zstd_alloc_used = 0;
}

void *mydos_zstd_malloc(unsigned int size) {
    unsigned int aligned = (size + 15u) & ~15u;
    if (aligned == 0 || aligned > ZSTD_ALLOC_ARENA_SIZE - zstd_alloc_used) {
        return 0;
    }
    void *result = zstd_alloc_arena + zstd_alloc_used;
    zstd_alloc_used += aligned;
    return result;
}

void *mydos_zstd_calloc(unsigned int count, unsigned int size) {
    if (size != 0 && count > 0xFFFFFFFFu / size) {
        return 0;
    }
    unsigned int total = count * size;
    void *result = mydos_zstd_malloc(total);
    if (result != 0) {
        memset(result, 0, total);
    }
    return result;
}

void mydos_zstd_free(void *pointer) {
    (void)pointer;
}
