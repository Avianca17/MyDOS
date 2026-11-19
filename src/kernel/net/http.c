#include <mydos.h>
#include <net/net.h>
#include <net/tcp.h>
#include <ff.h>

#define HTTP_HOST_MAX 128
#define HTTP_PATH_MAX 192
#define HTTP_HEADER_MAX 2048
#define HTTP_BUFFER_SIZE 1024
#define HTTP_RECV_TIMEOUT_MS 8000u

typedef struct {
    char host[HTTP_HOST_MAX];
    uint16_t port;
    char path[HTTP_PATH_MAX];
} http_url_t;

static int http_lower(int character) {
    if (character >= 'A' && character <= 'Z') {
        return character + ('a' - 'A');
    }
    return character;
}

static void http_copy(char *out, const char *text) {
    while (*text != '\0') {
        *out++ = *text++;
    }
    *out = '\0';
}

static int http_prefix(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text == '\0' || http_lower((unsigned char)*text) !=
                             http_lower((unsigned char)*prefix)) {
            return 0;
        }
        text++;
        prefix++;
    }
    return 1;
}

static int http_parse_uint(const char *text, uint32_t *value) {
    uint32_t result = 0;
    if (*text == '\0') {
        return 0;
    }
    while (*text >= '0' && *text <= '9') {
        result = result * 10u + (uint32_t)(*text - '0');
        text++;
    }
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    if (*text != '\0') {
        return 0;
    }
    *value = result;
    return 1;
}

static int http_parse_url(const char *url, http_url_t *out) {
    if (!http_prefix(url, "http://")) {
        return 0;
    }
    const char *host_start = url + 7;
    const char *host_end = strchr(host_start, '/');
    if (host_end == 0) {
        host_end = host_start + strlen(host_start);
    }
    uint32_t host_length = (uint32_t)(host_end - host_start);
    if (host_length == 0 || host_length >= HTTP_HOST_MAX) {
        return 0;
    }

    const char *port_separator = 0;
    for (const char *character = host_start; character < host_end; character++) {
        if (*character == ':') {
            port_separator = character;
        }
    }

    uint32_t actual_host_length = host_length;
    out->port = 80;
    if (port_separator != 0) {
        actual_host_length = (uint32_t)(port_separator - host_start);
        uint32_t port = 0;
        if (!http_parse_uint(port_separator + 1, &port) || port == 0 || port > 65535) {
            return 0;
        }
        out->port = (uint16_t)port;
    }
    if (actual_host_length == 0 || actual_host_length >= HTTP_HOST_MAX) {
        return 0;
    }
    memcpy(out->host, host_start, actual_host_length);
    out->host[actual_host_length] = '\0';

    if (*host_end == '/') {
        uint32_t path_length = (uint32_t)strlen(host_end);
        if (path_length == 0 || path_length >= HTTP_PATH_MAX) {
            return 0;
        }
        memcpy(out->path, host_end, path_length + 1);
    } else {
        http_copy(out->path, "/");
    }
    return 1;
}

static const char *http_header_end(const char *header) {
    const char *position = header;
    while (*position != '\0') {
        if (position[0] == '\r' && position[1] == '\n' &&
            position[2] == '\r' && position[3] == '\n') {
            return position + 4;
        }
        position++;
    }
    return 0;
}

static int http_header_value(const char *header, const char *name,
                             char *out, uint32_t capacity) {
    const char *line = header;
    while (*line != '\0') {
        const char *end = strchr(line, '\n');
        if (end == 0) {
            break;
        }
        if (http_prefix(line, name)) {
            const char *value = line + strlen(name);
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            uint32_t length = 0;
            while (*value != '\0' && *value != '\r' && *value != '\n' &&
                   length + 1 < capacity) {
                out[length++] = *value++;
            }
            out[length] = '\0';
            return 1;
        }
        line = end + 1;
    }
    return 0;
}

static int http_status_code(const char *header) {
    const char *space = strchr(header, ' ');
    if (space == 0 || space[1] < '0' || space[1] > '9') {
        return 0;
    }
    return (space[1] - '0') * 100 + (space[2] - '0') * 10 + (space[3] - '0');
}

static int http_append(char *out, uint32_t capacity, uint32_t *position,
                       const char *text) {
    while (*text != '\0') {
        if (*position + 1 >= capacity) {
            return 0;
        }
        out[(*position)++] = *text++;
    }
    return 1;
}

static int http_redirect_url(const http_url_t *source, const char *location,
                             char *out, uint32_t capacity) {
    if (http_prefix(location, "http://")) {
        uint32_t length = (uint32_t)strlen(location);
        if (length >= capacity) {
            return 0;
        }
        memcpy(out, location, length + 1);
        return 1;
    }
    if (location[0] != '/') {
        return 0;
    }
    uint32_t position = 0;
    if (!http_append(out, capacity, &position, "http://") ||
        !http_append(out, capacity, &position, source->host)) {
        return 0;
    }
    if (source->port != 80) {
        char port[16];
        port[0] = ':';
        uint32_t value = source->port;
        char digits[8];
        uint32_t count = 0;
        do {
            digits[count++] = (char)('0' + value % 10u);
            value /= 10u;
        } while (value != 0);
        uint32_t length = 1;
        for (uint32_t i = 0; i < count; i++) {
            port[length++] = digits[count - 1 - i];
        }
        port[length] = '\0';
        if (!http_append(out, capacity, &position, port)) {
            return 0;
        }
    }
    if (!http_append(out, capacity, &position, location)) {
        return 0;
    }
    out[position] = '\0';
    return 1;
}

static int http_write(FIL *file, const uint8_t *data, uint32_t length) {
    UINT written = 0;
    if (f_write(file, data, length, &written) != FR_OK || written != length) {
        return 0;
    }
    return 1;
}

static int http_get_file_internal(const char *url, const char *path, int depth) {
    http_url_t parsed;
    if (!http_parse_url(url, &parsed) || depth > 3) {
        return 0;
    }

    uint32_t server_ip;
    if (!net_parse_ipv4(parsed.host, &server_ip) &&
        !net_dns_resolve(parsed.host, &server_ip)) {
        print("flint: could not resolve host", RED, 0, 42);
        return 0;
    }

    tcp_socket_t socket;
    if (!tcp_connect(&socket, server_ip, parsed.port)) {
        print("flint: connection failed", RED, 0, 42);
        return 0;
    }

    char request[HTTP_HOST_MAX + HTTP_PATH_MAX + 128];
    uint32_t request_length = 0;
    http_append(request, sizeof(request), &request_length, "GET ");
    http_append(request, sizeof(request), &request_length, parsed.path);
    http_append(request, sizeof(request), &request_length, " HTTP/1.0\r\nHost: ");
    http_append(request, sizeof(request), &request_length, parsed.host);
    http_append(request, sizeof(request), &request_length,
                "\r\nConnection: close\r\nUser-Agent: MyDOS-flint/1.0\r\n\r\n");
    request[request_length] = '\0';
    if (!tcp_send(&socket, (const uint8_t *)request, (uint32_t)strlen(request))) {
        tcp_close(&socket);
        return 0;
    }

    static char header[HTTP_HEADER_MAX];
    static uint8_t buffer[HTTP_BUFFER_SIZE];
    uint32_t header_length = 0;
    uint32_t body_offset = 0;
    const char *body = 0;
    int closed = 0;

    while (header_length < HTTP_HEADER_MAX) {
        uint32_t received = 0;
        int result = tcp_recv(&socket, buffer, sizeof(buffer),
                              HTTP_RECV_TIMEOUT_MS, &received);
        if (result < 0) {
            tcp_close(&socket);
            print("flint: request timed out", RED, 0, 42);
            return 0;
        }
        if (result == 0) {
            closed = 1;
            break;
        }
        if (header_length + received > HTTP_HEADER_MAX) {
            received = HTTP_HEADER_MAX - header_length;
        }
        memcpy(header + header_length, buffer, received);
        header_length += received;
        header[header_length] = '\0';
        body = http_header_end(header);
        if (body != 0) {
            body_offset = (uint32_t)(body - header);
            break;
        }
    }

    if (body == 0) {
        tcp_close(&socket);
        return 0;
    }

    int status = http_status_code(header);
    if (status != 200) {
        char location[HTTP_PATH_MAX + HTTP_HOST_MAX + 32];
        char redirect[HTTP_PATH_MAX + HTTP_HOST_MAX + 32];
        if ((status == 301 || status == 302 || status == 303 || status == 307) &&
            http_header_value(header, "Location:", location, sizeof(location)) &&
            http_redirect_url(&parsed, location, redirect, sizeof(redirect))) {
            tcp_close(&socket);
            return http_get_file_internal(redirect, path, depth + 1);
        }
        tcp_close(&socket);
        print("flint: download failed", RED, 0, 42);
        return 0;
    }

    char transfer[32];
    if (http_header_value(header, "Transfer-Encoding:", transfer, sizeof(transfer)) &&
        http_prefix(transfer, "chunked")) {
        tcp_close(&socket);
        print("flint: chunked responses are not supported", RED, 0, 42);
        return 0;
    }

    uint32_t content_length = 0;
    char length_value[32];
    int has_length = http_header_value(header, "Content-Length:", length_value,
                                       sizeof(length_value)) &&
                     http_parse_uint(length_value, &content_length);

    FIL file;
    if (f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        tcp_close(&socket);
        print("flint: could not create file", RED, 0, 42);
        return 0;
    }

    uint32_t written_total = 0;
    uint32_t initial = header_length - body_offset;
    if (initial != 0 && !http_write(&file, (const uint8_t *)body, initial)) {
        f_close(&file);
        tcp_close(&socket);
        return 0;
    }
    written_total += initial;

    while (!closed && (!has_length || written_total < content_length)) {
        uint32_t received = 0;
        int result = tcp_recv(&socket, buffer, sizeof(buffer),
                              HTTP_RECV_TIMEOUT_MS, &received);
        if (result < 0) {
            break;
        }
        if (result == 0) {
            break;
        }
        uint32_t remaining = received;
        if (has_length && written_total + remaining > content_length) {
            remaining = content_length - written_total;
        }
        if (remaining != 0 && !http_write(&file, buffer, remaining)) {
            f_close(&file);
            tcp_close(&socket);
            return 0;
        }
        written_total += remaining;
    }

    f_close(&file);
    tcp_close(&socket);
    if (has_length && written_total < content_length) {
        return 0;
    }
    return 1;
}

int net_http_get_file(const char *url, const char *path) {
    return http_get_file_internal(url, path, 0);
}
