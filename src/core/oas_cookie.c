#include "oas_cookie.h"

#include <errno.h>
#include <stdckdint.h>
#include <string.h>

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* Percent-decode a string segment into an arena-allocated buffer */
static const char *percent_decode(oas_arena_t *arena, const char *src, size_t len)
{
    char *buf = oas_arena_alloc(arena, len + 1, 1);
    if (!buf) {
        return nullptr;
    }

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '%' && i + 2 < len) {
            int hi = hex_digit(src[i + 1]);
            int lo = hex_digit(src[i + 2]);
            if (hi >= 0 && lo >= 0) {
                char ch = (char)(hi * 16 + lo);
                if (ch != '\0') {
                    buf[j++] = ch;
                }
                i += 2;
                continue;
            }
        }
        buf[j++] = src[i];
    }
    buf[j] = '\0';
    return buf;
}

int oas_cookie_parse(const char *header, oas_arena_t *arena, oas_cookie_t **out, size_t *count)
{
    if (!arena || !out || !count) {
        return -EINVAL;
    }

    *out = nullptr;
    *count = 0;

    if (!header || *header == '\0') {
        return 0;
    }

    /* Count pairs: number of "; " separators + 1 */
    size_t n = 1;
    for (const char *p = header; *p; p++) {
        if (p[0] == ';' && p[1] == ' ') {
            n++;
        }
    }

    size_t alloc_size;
    if (ckd_mul(&alloc_size, sizeof(oas_cookie_t), n)) {
        return -EOVERFLOW;
    }
    oas_cookie_t *cookies = oas_arena_alloc(arena, alloc_size, _Alignof(oas_cookie_t));
    if (!cookies) {
        return -ENOMEM;
    }

    size_t idx = 0;
    const char *p = header;
    while (*p && idx < n) {
        /* Find end of this cookie pair ("; " separator) */
        const char *sep = strstr(p, "; ");
        size_t seg_len = sep ? (size_t)(sep - p) : strlen(p);

        /* Split on first '=' */
        const char *eq = memchr(p, '=', seg_len);
        if (eq) {
            size_t name_len = (size_t)(eq - p);
            size_t val_len = seg_len - name_len - 1;

            /* Cookie names are not percent-encoded per RFC 6265 */
            char *name_buf = oas_arena_alloc(arena, name_len + 1, 1);
            if (!name_buf) {
                return -ENOMEM;
            }
            memcpy(name_buf, p, name_len);
            name_buf[name_len] = '\0';
            cookies[idx].name = name_buf;

            cookies[idx].value = percent_decode(arena, eq + 1, val_len);
            if (!cookies[idx].value) {
                return -ENOMEM;
            }
        } else {
            /* No '=' — treat whole segment as name with empty value */
            char *name_buf = oas_arena_alloc(arena, seg_len + 1, 1);
            if (!name_buf) {
                return -ENOMEM;
            }
            memcpy(name_buf, p, seg_len);
            name_buf[seg_len] = '\0';
            cookies[idx].name = name_buf;
            cookies[idx].value = "";
        }
        idx++;

        if (sep) {
            p = sep + 2; /* skip "; " */
        } else {
            break;
        }
    }

    *out = cookies;
    *count = idx;
    return 0;
}
