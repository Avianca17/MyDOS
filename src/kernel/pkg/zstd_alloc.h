#ifndef MYDOS_ZSTD_ALLOC_H
#define MYDOS_ZSTD_ALLOC_H

#define ZSTD_MULTITHREAD 0
#define ZSTD_LEGACY_SUPPORT 0
#define ZSTD_LIB_DEPRECATED 0
#define ZSTD_NO_INLINE
#define ZSTD_DEPS_MALLOC
#define ZSTD_malloc(s) mydos_zstd_malloc(s)
#define ZSTD_calloc(n, s) mydos_zstd_calloc((n), (s))
#define ZSTD_free(p) mydos_zstd_free(p)

void *mydos_zstd_malloc(unsigned int size);
void *mydos_zstd_calloc(unsigned int count, unsigned int size);
void mydos_zstd_free(void *pointer);
void mydos_zstd_reset(void);

#endif
