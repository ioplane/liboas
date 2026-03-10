#include "oas_ref.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

/** Default timeout: 30 seconds */
constexpr int OAS_REF_FETCH_DEFAULT_TIMEOUT_MS = 30000;

/** Default max response size: 10 MB */
constexpr size_t OAS_REF_FETCH_DEFAULT_MAX_SIZE = 10ULL * 1024 * 1024;

/** Default max redirects: 5 hops */
constexpr int OAS_REF_FETCH_DEFAULT_MAX_REDIRECTS = 5;

/** Initial receive buffer size */
constexpr size_t OAS_REF_FETCH_INIT_BUF = 4096;

/**
 * Parse an HTTP URL into host, port, and path components.
 * Only supports http:// scheme. Returns -ENOTSUP for https://.
 */
static int parse_http_url(const char *url, char *host, size_t host_len, char *port, size_t port_len,
                          const char **path_out)
{
    if (strncmp(url, "https://", 8) == 0) {
        return -ENOTSUP;
    }

    if (strncmp(url, "http://", 7) != 0) {
        return -EINVAL;
    }

    const char *host_start = url + 7;

    /* Reject URLs with embedded CR/LF (HTTP header injection prevention) */
    for (const char *c = host_start; *c; c++) {
        if (*c == '\r' || *c == '\n') {
            return -EINVAL;
        }
    }

    const char *slash = strchr(host_start, '/');
    const char *colon = strchr(host_start, ':');

    /* Determine host end boundary */
    const char *host_end;
    if (colon && (!slash || colon < slash)) {
        host_end = colon;
    } else if (slash) {
        host_end = slash;
    } else {
        host_end = host_start + strlen(host_start);
    }

    size_t hlen = (size_t)(host_end - host_start);
    if (hlen == 0 || hlen >= host_len) {
        return -EINVAL;
    }
    memcpy(host, host_start, hlen);
    host[hlen] = '\0';

    /* Parse port */
    if (colon && (!slash || colon < slash)) {
        const char *port_start = colon + 1;
        const char *port_end = slash ? slash : port_start + strlen(port_start);
        size_t plen = (size_t)(port_end - port_start);
        if (plen == 0 || plen >= port_len) {
            return -EINVAL;
        }
        memcpy(port, port_start, plen);
        port[plen] = '\0';
    } else {
        (void)snprintf(port, port_len, "80");
    }

    /* Path defaults to "/" */
    *path_out = (slash && slash[0] != '\0') ? slash : "/";
    return 0;
}

/**
 * Find a header value in HTTP response headers (case-insensitive).
 * Returns pointer into headers or nullptr if not found.
 */
static const char *find_header(const char *headers, const char *name)
{
    size_t name_len = strlen(name);
    const char *p = headers;

    while (*p) {
        if (strncasecmp(p, name, name_len) == 0 && p[name_len] == ':') {
            const char *val = p + name_len + 1;
            while (*val == ' ' || *val == '\t') {
                val++;
            }
            return val;
        }
        /* Advance to next line */
        const char *eol = strstr(p, "\r\n");
        if (!eol) {
            break;
        }
        p = eol + 2;
    }
    return nullptr;
}

/**
 * Extract a header value as a newly allocated string (up to \r\n).
 * Caller must free() the result.
 */
static char *extract_header_value(const char *val_start)
{
    const char *end = strstr(val_start, "\r\n");
    if (!end) {
        end = val_start + strlen(val_start);
    }
    size_t len = (size_t)(end - val_start);
    char *result = malloc(len + 1);
    if (!result) {
        return nullptr;
    }
    memcpy(result, val_start, len);
    result[len] = '\0';
    return result;
}

/**
 * Parse HTTP status code from status line.
 * Expects "HTTP/1.x NNN ..."
 */
static int parse_status_code(const char *status_line)
{
    const char *space = strchr(status_line, ' ');
    if (!space) {
        return -1;
    }
    char *end;
    long code = strtol(space + 1, &end, 10);
    if (end == space + 1 || code < 100 || code > 999) {
        return -1;
    }
    return (int)code;
}

/**
 * Perform a single HTTP GET request and return the response body.
 * Does NOT follow redirects — caller handles that.
 */
static int do_http_get(const char *host, const char *port, const char *path, int timeout_ms,
                       size_t max_size, int *status_out, char **redirect_url, char **out_data,
                       size_t *out_len)
{
    int rc = 0;
    int sockfd = -1;
    char *buf = nullptr;
    struct addrinfo hints = {0};
    struct addrinfo *res = nullptr;

    *status_out = 0;
    *redirect_url = nullptr;
    *out_data = nullptr;
    *out_len = 0;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai_rc = getaddrinfo(host, port, &hints, &res);
    if (gai_rc != 0) {
        return -EHOSTUNREACH;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        rc = -errno;
        goto cleanup;
    }

    /* Set timeout for both send and receive */
    {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000L;
        (void)setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        (void)setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        rc = -errno;
        goto cleanup;
    }

    /* Send HTTP request */
    {
        char request[2048];
        int req_len = snprintf(request, sizeof(request),
                               "GET %s HTTP/1.1\r\n"
                               "Host: %s\r\n"
                               "User-Agent: liboas/0.1.0\r\n"
                               "Connection: close\r\n"
                               "\r\n",
                               path, host);
        if (req_len < 0 || (size_t)req_len >= sizeof(request)) {
            rc = -ENOMEM;
            goto cleanup;
        }

        ssize_t sent = send(sockfd, request, (size_t)req_len, 0);
        if (sent < 0 || (size_t)sent != (size_t)req_len) {
            rc = (sent < 0) ? -errno : -EIO;
            goto cleanup;
        }
    }

    /* Read response */
    {
        size_t buf_size = OAS_REF_FETCH_INIT_BUF;
        size_t total = 0;
        buf = malloc(buf_size);
        if (!buf) {
            rc = -ENOMEM;
            goto cleanup;
        }

        for (;;) {
            if (total >= buf_size - 1) {
                size_t new_size = buf_size * 2;
                if (new_size > max_size + OAS_REF_FETCH_INIT_BUF) {
                    rc = -EFBIG;
                    goto cleanup;
                }
                char *tmp = realloc(buf, new_size);
                if (!tmp) {
                    rc = -ENOMEM;
                    goto cleanup;
                }
                buf = tmp;
                buf_size = new_size;
            }

            ssize_t n = recv(sockfd, buf + total, buf_size - total - 1, 0);
            if (n < 0) {
                rc = -errno;
                goto cleanup;
            }
            if (n == 0) {
                break;
            }
            total += (size_t)n;
        }
        buf[total] = '\0';

        /* Parse headers from body */
        char *header_end = strstr(buf, "\r\n\r\n");
        if (!header_end) {
            rc = -EPROTO;
            goto cleanup;
        }

        *status_out = parse_status_code(buf);
        if (*status_out < 0) {
            rc = -EPROTO;
            goto cleanup;
        }

        /* Check for redirect */
        if (*status_out == 301 || *status_out == 302 || *status_out == 307 || *status_out == 308) {
            const char *loc = find_header(buf, "Location");
            if (loc) {
                *redirect_url = extract_header_value(loc);
            }
            goto cleanup;
        }

        /* Extract body */
        char *body = header_end + 4;
        size_t body_len = total - (size_t)(body - buf);

        if (body_len > max_size) {
            rc = -EFBIG;
            goto cleanup;
        }

        char *body_copy = malloc(body_len + 1);
        if (!body_copy) {
            rc = -ENOMEM;
            goto cleanup;
        }
        memcpy(body_copy, body, body_len);
        body_copy[body_len] = '\0';

        *out_data = body_copy;
        *out_len = body_len;
    }

cleanup:
    free(buf);
    if (res) {
        freeaddrinfo(res);
    }
    if (sockfd >= 0) {
        (void)close(sockfd);
    }
    return rc;
}

int oas_ref_fetch_http(const char *url, int timeout_ms, size_t max_size, int max_redirects,
                       char **out_data, size_t *out_len)
{
    if (!url || !out_data || !out_len) {
        return -EINVAL;
    }

    *out_data = nullptr;
    *out_len = 0;

    if (timeout_ms <= 0) {
        timeout_ms = OAS_REF_FETCH_DEFAULT_TIMEOUT_MS;
    }
    if (max_size == 0) {
        max_size = OAS_REF_FETCH_DEFAULT_MAX_SIZE;
    }
    if (max_redirects <= 0) {
        max_redirects = OAS_REF_FETCH_DEFAULT_MAX_REDIRECTS;
    }

    /* Work with a mutable copy of the URL for redirect following */
    char *current_url = strdup(url);
    if (!current_url) {
        return -ENOMEM;
    }

    int rc = 0;
    int redirects_left = max_redirects;

    while (redirects_left >= 0) {
        char host[256];
        char port[8];
        const char *path;

        rc = parse_http_url(current_url, host, sizeof(host), port, sizeof(port), &path);
        if (rc < 0) {
            goto done;
        }

        int status = 0;
        char *redirect_url = nullptr;

        rc = do_http_get(host, port, path, timeout_ms, max_size, &status, &redirect_url, out_data,
                         out_len);
        if (rc < 0) {
            free(redirect_url);
            goto done;
        }

        /* Follow redirect */
        if (redirect_url) {
            free(current_url);
            current_url = redirect_url;
            redirects_left--;
            if (redirects_left < 0) {
                rc = -ELOOP;
                goto done;
            }
            continue;
        }

        /* Non-2xx status → error */
        if (status < 200 || status >= 300) {
            free(*out_data);
            *out_data = nullptr;
            *out_len = 0;
            rc = -EPROTO;
            goto done;
        }

        /* Success */
        break;
    }

done:
    free(current_url);
    return rc;
}
