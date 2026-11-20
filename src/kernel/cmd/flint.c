#include <mydos.h>
#include <cmd/cmd.h>
#include <ff.h>
#include <net/net.h>
#include <pkg/pkg.h>

#define FLINT_INDEX_URL "http://distfiles.gentoo.org/releases/x86/binpackages/23.0/i686/Packages"
#define FLINT_PKG_BASE "http://distfiles.gentoo.org/releases/x86/binpackages/23.0/i686/"
#define FLINT_INDEX_PATH "FLINT/PACKAGES.TXT"
#define FLINT_PKG_DIR "FLINT/PACKAGES"
#define FLINT_LINE_MAX 256

static void flint_copy(char *out, const char *text) {
    while (*text != '\0') {
        *out++ = *text++;
    }
    *out = '\0';
}

static int flint_lower(int character) {
    if (character >= 'A' && character <= 'Z') {
        return character + ('a' - 'A');
    }
    return character;
}

static int flint_prefix(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text == '\0' || flint_lower((unsigned char)*text) !=
                             flint_lower((unsigned char)*prefix)) {
            return 0;
        }
        text++;
        prefix++;
    }
    return 1;
}

static int flint_contains(const char *text, const char *needle) {
    if (*needle == '\0') {
        return 1;
    }
    while (*text != '\0') {
        const char *candidate = text;
        const char *search = needle;
        while (*candidate != '\0' && *search != '\0' &&
               flint_lower((unsigned char)*candidate) ==
               flint_lower((unsigned char)*search)) {
            candidate++;
            search++;
        }
        if (*search == '\0') {
            return 1;
        }
        text++;
    }
    return 0;
}

static int flint_field(const char *line, const char *name,
                       char *out, uint32_t capacity) {
    if (!flint_prefix(line, name)) {
        return 0;
    }
    const char *value = line + strlen(name);
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    uint32_t position = 0;
    while (*value != '\0' && *value != '\r' && *value != '\n' &&
           position + 1 < capacity) {
        out[position++] = *value++;
    }
    out[position] = '\0';
    return 1;
}

static void flint_line(const char *text, unsigned char color, int *cursor_y) {
    print(text, color, 0, *cursor_y);
    scrl(cursor_y);
}

static void flint_ensure_dirs(void) {
    if (f_mkdir("FLINT") != FR_OK && f_mkdir("FLINT") != FR_EXIST) {
        return;
    }
    f_mkdir(FLINT_PKG_DIR);
}

static int flint_read_line(FIL *file, char *out, uint32_t capacity) {
    uint32_t position = 0;
    BYTE character;
    UINT read = 0;

    while (f_read(file, &character, 1, &read) == FR_OK && read == 1) {
        if (character == '\r') {
            continue;
        }
        if (character == '\n') {
            out[position] = '\0';
            return 1;
        }
        if (position + 1 < capacity) {
            out[position++] = (char)character;
        }
    }
    out[position] = '\0';
    return position != 0;
}

static int flint_open_index(FIL *file, int *cursor_y) {
    if (f_open(file, FLINT_INDEX_PATH, FA_READ) != FR_OK) {
        flint_line("flint: no cached index; run 'flint sync' first", RED, cursor_y);
        return 0;
    }
    return 1;
}

static void flint_help(int *cursor_y) {
    flint_line("Usage: flint <command> [arguments]", WHITE, cursor_y);
    flint_line("  sync                 download Gentoo x86 package index", LIGHT_GREY, cursor_y);
    flint_line("  search <term>        search cached package names", LIGHT_GREY, cursor_y);
    flint_line("  info <atom>          show package metadata", LIGHT_GREY, cursor_y);
    flint_line("  install <atom>       download and install a .gpkg.tar package", LIGHT_GREY, cursor_y);
    flint_line("  fetch <url> <dest>   download any plain HTTP URL", LIGHT_GREY, cursor_y);
    flint_line("  source               show package source URL", LIGHT_GREY, cursor_y);
    flint_line("Packages are unpacked into BIN, USR, and ETC.", YELLOW, cursor_y);
}

static void flint_sync(int *cursor_y) {
    flint_ensure_dirs();
    flint_line("flint: syncing Gentoo x86 package index...", WHITE, cursor_y);
    if (net_http_get_file(FLINT_INDEX_URL, FLINT_INDEX_PATH)) {
        flint_line("flint: index saved to FLINT/PACKAGES.TXT", GREEN, cursor_y);
    } else {
        flint_line("flint: sync failed", RED, cursor_y);
    }
}

static void flint_source(int *cursor_y) {
    flint_line("flint source: " FLINT_INDEX_URL, WHITE, cursor_y);
    flint_line("packages base: " FLINT_PKG_BASE, LIGHT_GREY, cursor_y);
}

static void flint_fetch(char **input, int *cursor_y) {
    char *url = cmd_next_token(input);
    char *destination = cmd_next_token(input);
    if (url == 0 || destination == 0) {
        flint_line("Usage: flint fetch <url> <dest>", WHITE, cursor_y);
        return;
    }
    flint_line("flint: downloading...", WHITE, cursor_y);
    if (net_http_get_file(url, destination)) {
        flint_line("flint: download complete", GREEN, cursor_y);
    } else {
        flint_line("flint: download failed", RED, cursor_y);
    }
}

static void flint_search(const char *term, int *cursor_y) {
    FIL file;
    if (!flint_open_index(&file, cursor_y)) {
        return;
    }

    char line[FLINT_LINE_MAX];
    int shown = 0;
    flint_line("flint: matches:", WHITE, cursor_y);
    while (flint_read_line(&file, line, sizeof(line))) {
        char cpv[128];
        if (flint_field(line, "CPV:", cpv, sizeof(cpv)) && flint_contains(cpv, term)) {
            print(cpv, LIGHT_GREY, 2, *cursor_y);
            scrl(cursor_y);
            shown++;
            if (shown >= 24) {
                break;
            }
        }
    }
    f_close(&file);
    if (shown == 0) {
        flint_line("flint: no matches", RED, cursor_y);
    }
}

static int flint_find_value(const char *query, const char *field_name,
                            char *out, uint32_t capacity, int exact) {
    FIL file;
    if (f_open(&file, FLINT_INDEX_PATH, FA_READ) != FR_OK) {
        return 0;
    }
    char line[FLINT_LINE_MAX];
    char cpv[128];
    int found = 0;

    while (flint_read_line(&file, line, sizeof(line))) {
        if (flint_field(line, "CPV:", cpv, sizeof(cpv))) {
            int matches = exact ? strcmp(cpv, query) == 0 : flint_contains(cpv, query);
            found = matches;
            continue;
        }
        if (found && flint_field(line, field_name, out, capacity)) {
            f_close(&file);
            return 1;
        }
    }
    f_close(&file);
    return 0;
}

static void flint_info(const char *query, int *cursor_y) {
    FIL file;
    if (!flint_open_index(&file, cursor_y)) {
        return;
    }

    char line[FLINT_LINE_MAX];
    char cpv[128];
    int inside = 0;
    int shown = 0;

    while (flint_read_line(&file, line, sizeof(line))) {
        if (flint_field(line, "CPV:", cpv, sizeof(cpv))) {
            inside = strcmp(cpv, query) == 0;
            if (!inside) {
                inside = flint_contains(cpv, query);
            }
            if (inside) {
                print(line, WHITE, 0, *cursor_y);
                scrl(cursor_y);
                shown++;
            }
            continue;
        }
        if (inside) {
            if (line[0] == '\0') {
                break;
            }
            print(line, LIGHT_GREY, 0, *cursor_y);
            scrl(cursor_y);
            shown++;
            if (shown >= 18) {
                break;
            }
        }
    }
    f_close(&file);
    if (shown == 0) {
        flint_line("flint: package not found", RED, cursor_y);
    }
}

static void flint_install(const char *query, int *cursor_y) {
    char package_path[192];
    if (!flint_find_value(query, "PATH:", package_path, sizeof(package_path), 1) &&
        !flint_find_value(query, "PATH:", package_path, sizeof(package_path), 0)) {
        flint_line("flint: package not found", RED, cursor_y);
        return;
    }

    const char *filename = package_path;
    for (const char *character = package_path; *character != '\0'; character++) {
        if (*character == '/') {
            filename = character + 1;
        }
    }

    char url[sizeof(FLINT_PKG_BASE) + 192];
    char destination[sizeof(FLINT_PKG_DIR) + 192];
    flint_copy(url, FLINT_PKG_BASE);
    uint32_t url_length = strlen(url);
    flint_copy(url + url_length, package_path);
    flint_copy(destination, FLINT_PKG_DIR "/");
    uint32_t destination_length = strlen(destination);
    flint_copy(destination + destination_length, filename);

    flint_ensure_dirs();
    flint_line("flint: downloading " , WHITE, cursor_y);
    print(package_path, LIGHT_GREY, 18, *cursor_y - 1);
    if (!net_http_get_file(url, destination)) {
        flint_line("flint: package download failed", RED, cursor_y);
        return;
    }

    char executable[64];
    int installed = pkg_install_gpkg(destination, executable, sizeof(executable));
    if (installed <= 0) {
        flint_line("flint: could not install package", RED, cursor_y);
        return;
    }

    flint_line("flint: installed files from downloaded package", GREEN, cursor_y);
    if (executable[0] != '\0') {
        flint_line("flint: run ", WHITE, cursor_y);
        print(executable, LIGHT_GREY, 11, *cursor_y - 1);
    }
}

void cmd_flint(char *args, int *cursor_y) {
    char *input = args;
    char *command = cmd_next_token(&input);

    if (command == 0 || strcmp(command, "help") == 0) {
        flint_help(cursor_y);
        return;
    }
    if (strcmp(command, "sync") == 0) {
        flint_sync(cursor_y);
        return;
    }
    if (strcmp(command, "source") == 0) {
        flint_source(cursor_y);
        return;
    }
    if (strcmp(command, "fetch") == 0) {
        flint_fetch(&input, cursor_y);
        return;
    }

    char *value = cmd_next_token(&input);
    if (value == 0) {
        flint_line("Usage: flint <sync|search|info|install|fetch|source>", WHITE, cursor_y);
        return;
    }
    if (strcmp(command, "search") == 0) {
        flint_search(value, cursor_y);
    } else if (strcmp(command, "info") == 0) {
        flint_info(value, cursor_y);
    } else if (strcmp(command, "install") == 0) {
        flint_install(value, cursor_y);
    } else {
        flint_line("flint: unknown subcommand", RED, cursor_y);
    }
}
