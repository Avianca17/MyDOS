#ifndef MYDOS_PKG_H
#define MYDOS_PKG_H

int pkg_install_gpkg(const char *archive_path, char *executable_path,
                     unsigned int executable_capacity);
int mydos_zstd_decompress(const unsigned char *source, unsigned int source_size,
                          unsigned char *destination, unsigned int capacity,
                          unsigned int *destination_size);

#endif
