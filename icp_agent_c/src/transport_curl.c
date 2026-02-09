#include "icp_agent/transport.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} dynbuf_t;

static int dynbuf_reserve(dynbuf_t *buf, size_t add) {
    size_t   needed = buf->len + add;
    size_t   cap = (buf->cap == 0) ? 256 : buf->cap;
    uint8_t *next = NULL;
    if (needed <= buf->cap)
        return 1;
    while (cap < needed)
        cap *= 2;
    next = (uint8_t *)realloc(buf->data, cap);
    if (next == NULL)
        return 0;
    buf->data = next;
    buf->cap = cap;
    return 1;
}

static int dynbuf_append(dynbuf_t *buf, const uint8_t *src, size_t n) {
    if (!dynbuf_reserve(buf, n))
        return 0;
    memcpy(buf->data + buf->len, src, n);
    buf->len += n;
    return 1;
}

static const uint8_t *find_header_separator(const uint8_t *buf, size_t len) {
    if (buf == NULL || len < 4)
        return NULL;
    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' &&
            buf[i + 3] == '\n') {
            return buf + i;
        }
    }
    return NULL;
}

static int
parse_status_code(const uint8_t *head, size_t head_len, long *status) {
    size_t i = 0;
    size_t first_space = 0;
    size_t second_space = 0;
    char   num[4] = {0};
    if (head == NULL || status == NULL || head_len < 12)
        return 0;
    for (i = 0; i < head_len; i++) {
        if (head[i] == ' ') {
            first_space = i;
            break;
        }
    }
    if (first_space == 0 || first_space + 4 >= head_len)
        return 0;
    for (i = first_space + 1; i < head_len; i++) {
        if (head[i] == ' ') {
            second_space = i;
            break;
        }
    }
    if (second_space == 0 || second_space - first_space != 4)
        return 0;
    memcpy(num, head + first_space + 1, 3);
    *status = strtol(num, NULL, 10);
    return 1;
}

static int
parse_http_url(const char *url, char **host, char **port, char **path) {
    const char *scheme = "http://";
    const char *p = NULL;
    const char *slash = NULL;
    const char *colon = NULL;
    size_t      host_len = 0;

    if (strncmp(url, scheme, strlen(scheme)) != 0)
        return 0;
    p = url + strlen(scheme);
    slash = strchr(p, '/');
    if (slash == NULL)
        return 0;
    colon = memchr(p, ':', (size_t)(slash - p));

    if (colon != NULL) {
        host_len = (size_t)(colon - p);
        *host = (char *)malloc(host_len + 1);
        *port = (char *)malloc((size_t)(slash - colon));
        if (*host == NULL || *port == NULL)
            return 0;
        memcpy(*host, p, host_len);
        (*host)[host_len] = '\0';
        memcpy(*port, colon + 1, (size_t)(slash - colon - 1));
        (*port)[(size_t)(slash - colon - 1)] = '\0';
    } else {
        host_len = (size_t)(slash - p);
        *host = (char *)malloc(host_len + 1);
        *port = (char *)malloc(3);
        if (*host == NULL || *port == NULL)
            return 0;
        memcpy(*host, p, host_len);
        (*host)[host_len] = '\0';
        strcpy(*port, "80");
    }

    *path = strdup(slash);
    return *path != NULL;
}

static int connect_tcp(const char *host, const char *port) {
    struct addrinfo  hints;
    struct addrinfo *res = NULL;
    struct addrinfo *it = NULL;
    int              fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0)
        return -1;

    for (it = res; it != NULL; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

ic_agent_error_code_t ic_http_post_binary(const char         *url,
                                          const uint8_t      *body,
                                          size_t              body_len,
                                          ic_http_response_t *out_response) {
    char          *host = NULL;
    char          *port = NULL;
    char          *path = NULL;
    int            fd = -1;
    char           req_header[512];
    int            req_len = 0;
    dynbuf_t       raw = {0};
    uint8_t        tmp[2048];
    ssize_t        n = 0;
    const uint8_t *sep = NULL;
    size_t         head_len = 0;
    long           status = 0;

    if (url == NULL || body == NULL || out_response == NULL)
        return IC_AGENT_ERR;
    out_response->status_code = 0;
    out_response->body = NULL;
    out_response->body_len = 0;

    if (!parse_http_url(url, &host, &port, &path))
        goto fail;
    fd = connect_tcp(host, port);
    if (fd < 0)
        goto fail;

    req_len = snprintf(req_header, sizeof(req_header),
                       "POST %s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "Content-Type: application/cbor\r\n"
                       "Accept: application/cbor\r\n"
                       "Content-Length: %zu\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       path, host, body_len);
    if (req_len <= 0 || (size_t)req_len >= sizeof(req_header))
        goto fail;

    if (send(fd, req_header, (size_t)req_len, 0) != req_len)
        goto fail;
    if (body_len > 0 && send(fd, body, body_len, 0) != (ssize_t)body_len)
        goto fail;

    for (;;) {
        n = recv(fd, tmp, sizeof(tmp), 0);
        if (n == 0)
            break;
        if (n < 0) {
            if (errno == ECONNRESET && raw.len > 0) {
                break;
            }
            goto fail;
        }
        if (!dynbuf_append(&raw, tmp, (size_t)n))
            goto fail;
    }

    sep = find_header_separator(raw.data, raw.len);
    if (sep == NULL)
        goto fail;
    head_len = (size_t)(sep - raw.data);
    if (!parse_status_code(raw.data, head_len, &status))
        goto fail;

    out_response->status_code = status;
    out_response->body_len = raw.len - (head_len + 4);
    out_response->body = (uint8_t *)malloc(out_response->body_len);
    if (out_response->body == NULL)
        goto fail;
    memcpy(out_response->body, sep + 4, out_response->body_len);

    close(fd);
    free(host);
    free(port);
    free(path);
    free(raw.data);
    return IC_AGENT_OK;

fail:
    if (fd >= 0)
        close(fd);
    free(host);
    free(port);
    free(path);
    free(raw.data);
    if (out_response->body != NULL) {
        free(out_response->body);
        out_response->body = NULL;
    }
    out_response->body_len = 0;
    out_response->status_code = 0;
    return IC_AGENT_ERR;
}

void ic_http_response_free(ic_http_response_t *response) {
    if (response == NULL)
        return;
    free(response->body);
    response->body = NULL;
    response->body_len = 0;
    response->status_code = 0;
}
