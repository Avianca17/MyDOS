#include <mydos.h>
#include <ff.h>
#include <pkg/pkg.h>

#define GPKG_ARCHIVE_MAX (8u * 1024u * 1024u)
#define GPKG_IMAGE_MAX (24u * 1024u * 1024u)
#define GPKG_TAR_BLOCK 512u
#define GPKG_PATH_MAX 192u

static unsigned char gpkg_archive[GPKG_ARCHIVE_MAX];
static unsigned char gpkg_image[GPKG_IMAGE_MAX];
static unsigned int gpkg_image_size;

static int gpkg_parse_octal(const unsigned char *field, unsigned int length,
                            unsigned int *value) {
    unsigned int parsed = 0;
    int found_digit = 0;
    int ended = 0;

    for (unsigned int i = 0; i < length; i++) {
        unsigned char character = field[i];
        if (character == '\0' || character == ' ') {
            if (found_digit) {
                ended = 1;
            }
            continue;
        }
        if (character < '0' || character > '7' || ended) {
            return 0;
        }
        if (parsed > (0xFFFFFFFFu - (unsigned int)(character - '0')) / 8u) {
            return 0;
        }
        parsed = parsed * 8u + (unsigned int)(character - '0');
        found_digit = 1;
    }
    *value = parsed;
    return found_digit;
}

static int gpkg_header_valid(const unsigned char *header) {
    unsigned int expected;
    unsigned int actual = 0;

    if (!gpkg_parse_octal(header + 148, 8, &expected)) {
        return 0;
    }
    for (unsigned int i = 0; i < GPKG_TAR_BLOCK; i++) {
        actual += (i >= 148 && i < 156) ? (unsigned int)' ' : header[i];
    }
    return actual == expected;
}

static unsigned int gpkg_field_length(const unsigned char *field,
                                      unsigned int limit) {
    unsigned int length = 0;
    while (length < limit && field[length] != '\0') {
        length++;
    }
    return length;
}

static int gpkg_tar_path(const unsigned char *header, char *path,
                         unsigned int capacity) {
    unsigned int name_length = gpkg_field_length(header, 100);
    unsigned int prefix_length = gpkg_field_length(header + 345, 155);
    unsigned int position = 0;

    if (name_length == 0) {
        return 0;
    }
    if (prefix_length != 0) {
        if (prefix_length + 1 + name_length >= capacity) {
            return 0;
        }
        memcpy(path, header + 345, prefix_length);
        position = prefix_length;
        path[position++] = '/';
    } else if (name_length >= capacity) {
        return 0;
    }
    memcpy(path + position, header, name_length);
    position += name_length;
    path[position] = '\0';

    while (position > 1 && path[position - 1] == '/') {
        path[--position] = '\0';
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 0;
    }
    for (unsigned int start = 0, i = 0; i <= position; i++) {
        if (path[i] == '\\') {
            return 0;
        }
        if (path[i] == '/' || path[i] == '\0') {
            unsigned int length = i - start;
            if (length == 2 && path[start] == '.' && path[start + 1] == '.') {
                return 0;
            }
            if (length == 1 && path[start] == '.') {
                return 0;
            }
            start = i + 1;
        }
    }
    return 1;
}

static int gpkg_suffix(const char *text, const char *suffix) {
    unsigned int text_length = (unsigned int)strlen(text);
    unsigned int suffix_length = (unsigned int)strlen(suffix);
    return text_length >= suffix_length &&
           strcmp(text + text_length - suffix_length, suffix) == 0;
}

static int gpkg_decompress_image(void) {
    unsigned int offset = 0;

    while (GPKG_ARCHIVE_MAX - offset >= GPKG_TAR_BLOCK) {
        const unsigned char *header = gpkg_archive + offset;
        int empty = 1;
        for (unsigned int i = 0; i < GPKG_TAR_BLOCK; i++) {
            if (header[i] != 0) {
                empty = 0;
                break;
            }
        }
        if (empty) {
            break;
        }
        if (!gpkg_header_valid(header)) {
            return 0;
        }

        unsigned int file_size;
        if (!gpkg_parse_octal(header + 124, 12, &file_size)) {
            return 0;
        }
        unsigned int data_offset = offset + GPKG_TAR_BLOCK;
        unsigned int padded_size = (file_size + GPKG_TAR_BLOCK - 1u) &
                                   ~(GPKG_TAR_BLOCK - 1u);
        if (data_offset < offset ||
            padded_size > GPKG_ARCHIVE_MAX - data_offset) {
            return 0;
        }

        char name[GPKG_PATH_MAX];
        if (gpkg_tar_path(header, name, sizeof(name)) &&
            gpkg_suffix(name, "image.tar.zst")) {
            return mydos_zstd_decompress(gpkg_archive + data_offset, file_size,
                                         gpkg_image, GPKG_IMAGE_MAX,
                                         &gpkg_image_size);
        }

        offset = data_offset + padded_size;
    }
    return 0;
}

static int gpkg_make_directories(const char *path) {
    char directory[GPKG_PATH_MAX];
    unsigned int length = (unsigned int)strlen(path);

    if (length == 0 || length >= sizeof(directory)) {
        return 0;
    }
    memcpy(directory, path, length + 1);
    for (unsigned int i = 1; i <= length; i++) {
        if (directory[i] == '/' || directory[i] == '\0') {
            if (directory[i] == '\0' && i == length) {
                break;
            }
            char saved = directory[i];
            directory[i] = '\0';
            FRESULT result = f_mkdir(directory);
            if (result != FR_OK && result != FR_EXIST) {
                return 0;
            }
            directory[i] = saved;
        }
    }
    return 1;
}

static int gpkg_write_file(const unsigned char *data, unsigned int size,
                           const char *path) {
    if (!gpkg_make_directories(path)) {
        return 0;
    }

    FIL file;
    if (f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        return 0;
    }

    unsigned int offset = 0;
    while (offset < size) {
        UINT chunk = size - offset > 4096u ? 4096u : (UINT)(size - offset);
        UINT written = 0;
        if (f_write(&file, data + offset, chunk, &written) != FR_OK ||
            written != chunk) {
            f_close(&file);
            return 0;
        }
        offset += chunk;
    }
    return f_close(&file) == FR_OK;
}

static int gpkg_map_path(const char *source, char *destination,
                         unsigned int capacity) {
    static const char *bin_prefixes[] = { "usr/bin/", "usr/sbin/", "bin/", "sbin/" };
    static const char *lib_prefixes[] = { "usr/lib/", "usr/lib32/", "lib/", "lib32/" };

    for (unsigned int i = 0; i < sizeof(bin_prefixes) / sizeof(bin_prefixes[0]); i++) {
        unsigned int prefix_length = (unsigned int)strlen(bin_prefixes[i]);
        if (strncmp(source, bin_prefixes[i], prefix_length) == 0) {
            if (prefix_length + 4u + strlen(source + prefix_length) >= capacity) {
                return 0;
            }
            memcpy(destination, "BIN/", 4);
            memcpy(destination + 4, source + prefix_length,
                   strlen(source + prefix_length) + 1);
            return 1;
        }
    }

    for (unsigned int i = 0; i < sizeof(lib_prefixes) / sizeof(lib_prefixes[0]); i++) {
        unsigned int prefix_length = (unsigned int)strlen(lib_prefixes[i]);
        if (strncmp(source, lib_prefixes[i], prefix_length) == 0) {
            if (prefix_length + 9u + strlen(source + prefix_length) >= capacity) {
                return 0;
            }
            memcpy(destination, "USR/LIB/", 8);
            memcpy(destination + 8, source + prefix_length,
                   strlen(source + prefix_length) + 1);
            return 1;
        }
    }

    if (strncmp(source, "usr/share/", 10) == 0) {
        if (10u + 11u + strlen(source + 10) >= capacity) {
            return 0;
        }
        memcpy(destination, "USR/SHARE/", 10);
        memcpy(destination + 10, source + 10, strlen(source + 10) + 1);
        return 1;
    }
    if (strncmp(source, "usr/", 4) == 0) {
        if (4u + 4u + strlen(source + 4) >= capacity) {
            return 0;
        }
        memcpy(destination, "USR/", 4);
        memcpy(destination + 4, source + 4, strlen(source + 4) + 1);
        return 1;
    }
    if (strncmp(source, "etc/", 4) == 0) {
        if (4u + 4u + strlen(source + 4) >= capacity) {
            return 0;
        }
        memcpy(destination, "ETC/", 4);
        memcpy(destination + 4, source + 4, strlen(source + 4) + 1);
        return 1;
    }

    if (strlen(source) + 1 >= capacity) {
        return 0;
    }
    memcpy(destination, source, strlen(source) + 1);
    return 1;
}

static void gpkg_copy_basename(char *out, unsigned int capacity,
                               const char *path) {
    const char *name = path;
    for (const char *character = path; *character != '\0'; character++) {
        if (*character == '/') {
            name = character + 1;
        }
    }
    unsigned int length = (unsigned int)strlen(name);
    if (length >= capacity) {
        length = capacity - 1;
    }
    memcpy(out, name, length);
    out[length] = '\0';
}

static int gpkg_install_image(char *executable_path,
                              unsigned int executable_capacity) {
    unsigned int offset = 0;
    int installed = 0;

    if (executable_path != 0 && executable_capacity != 0) {
        executable_path[0] = '\0';
    }

    while (GPKG_IMAGE_MAX - offset >= GPKG_TAR_BLOCK) {
        const unsigned char *header = gpkg_image + offset;
        int empty = 1;
        for (unsigned int i = 0; i < GPKG_TAR_BLOCK; i++) {
            if (header[i] != 0) {
                empty = 0;
                break;
            }
        }
        if (empty) {
            break;
        }
        if (!gpkg_header_valid(header)) {
            return 0;
        }

        unsigned int file_size;
        if (!gpkg_parse_octal(header + 124, 12, &file_size)) {
            return 0;
        }
        unsigned int data_offset = offset + GPKG_TAR_BLOCK;
        unsigned int padded_size = (file_size + GPKG_TAR_BLOCK - 1u) &
                                   ~(GPKG_TAR_BLOCK - 1u);
        if (data_offset < offset || data_offset > gpkg_image_size ||
            padded_size > gpkg_image_size - data_offset) {
            return 0;
        }

        char name[GPKG_PATH_MAX];
        unsigned char type = header[156];
        if (gpkg_tar_path(header, name, sizeof(name)) &&
            (type == '\0' || type == '0' || type == '5')) {
            const char *relative = name;
            if (strncmp(relative, "image/", 6) == 0) {
                relative += 6;
            }
            if (*relative != '\0') {
                char destination[GPKG_PATH_MAX];
                if (gpkg_map_path(relative, destination, sizeof(destination))) {
                    if (type == '5') {
                        unsigned int length = (unsigned int)strlen(destination);
                        while (length > 1 && destination[length - 1] == '/') {
                            destination[--length] = '\0';
                        }
                        if (gpkg_make_directories(destination)) {
                            installed++;
                        }
                    } else if (gpkg_write_file(gpkg_image + data_offset,
                                               file_size, destination)) {
                        installed++;
                        if (executable_path != 0 && executable_capacity != 0 &&
                            executable_path[0] == '\0' &&
                            (strncmp(destination, "BIN/", 4) == 0 ||
                             strncmp(destination, "USR/BIN/", 8) == 0)) {
                            gpkg_copy_basename(executable_path,
                                               executable_capacity,
                                               destination);
                        }
                    }
                }
            }
        }

        offset = data_offset + padded_size;
    }
    return installed;
}

int pkg_install_gpkg(const char *archive_path, char *executable_path,
                     unsigned int executable_capacity) {
    FIL file;
    if (f_open(&file, archive_path, FA_READ) != FR_OK) {
        return 0;
    }
    if (f_size(&file) > GPKG_ARCHIVE_MAX) {
        f_close(&file);
        return 0;
    }

    UINT bytes_read = 0;
    if (f_read(&file, gpkg_archive, GPKG_ARCHIVE_MAX, &bytes_read) != FR_OK) {
        f_close(&file);
        return 0;
    }
    f_close(&file);
    if (bytes_read == 0 || bytes_read > GPKG_ARCHIVE_MAX) {
        return 0;
    }

    if (!gpkg_decompress_image()) {
        return 0;
    }
    return gpkg_install_image(executable_path, executable_capacity);
}
