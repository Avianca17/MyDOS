#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <pkg/pkg.h>
#include "zstd_alloc.h"

static void *zstd_custom_alloc(void *opaque, size_t size) {
    (void)opaque;
    return mydos_zstd_malloc((unsigned int)size);
}

static void zstd_custom_free(void *opaque, void *pointer) {
    (void)opaque;
    mydos_zstd_free(pointer);
}

int mydos_zstd_decompress(const unsigned char *source, unsigned int source_size,
                          unsigned char *destination, unsigned int capacity,
                          unsigned int *destination_size) {
    if (source == 0 || destination == 0 || destination_size == 0) {
        return 0;
    }
    *destination_size = 0;

    mydos_zstd_reset();
    ZSTD_customMem memory = { zstd_custom_alloc, zstd_custom_free, 0 };
    ZSTD_DCtx *context = ZSTD_createDCtx_advanced(memory);
    if (context == 0) {
        return 0;
    }

    size_t result = ZSTD_decompressDCtx(context, destination, capacity,
                                        source, source_size);
    ZSTD_freeDCtx(context);
    if (ZSTD_isError(result) || result > capacity) {
        return 0;
    }
    *destination_size = (unsigned int)result;
    return 1;
}
