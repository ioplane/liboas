#include "oas_query.h"

#include <errno.h>
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

/* Decode percent-encoded string in-place into arena-allocated buffer */
static const char *url_decode(oas_arena_t *arena, const char *src, size_t len)
{
    /* Allocate worst case (same length) */
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
                buf[j++] = (char)(hi * 16 + lo);
                i += 2;
                continue;
            }
        } else if (src[i] == '+') {
            buf[j++] = ' ';
            continue;
        }
        buf[j++] = src[i];
    }
    buf[j] = '\0';
    return buf;
}

int oas_query_parse(oas_arena_t *arena, const char *query_string, oas_query_pair_t **out,
                    size_t *out_count)
{
    if (!arena || !out || !out_count) {
        return -EINVAL;
    }

    *out = nullptr;
    *out_count = 0;

    if (!query_string || *query_string == '\0') {
        return 0;
    }

    /* Count pairs (number of '&' + 1) */
    size_t count = 1;
    for (const char *p = query_string; *p; p++) {
        if (*p == '&') {
            count++;
        }
    }

    oas_query_pair_t *pairs =
        oas_arena_alloc(arena, sizeof(*pairs) * count, _Alignof(oas_query_pair_t));
    if (!pairs) {
        return -ENOMEM;
    }

    size_t idx = 0;
    const char *p = query_string;
    while (*p && idx < count) {
        const char *amp = strchr(p, '&');
        size_t seg_len = amp ? (size_t)(amp - p) : strlen(p);

        const char *eq = memchr(p, '=', seg_len);
        if (eq) {
            size_t key_len = (size_t)(eq - p);
            size_t val_len = seg_len - key_len - 1;
            pairs[idx].key = url_decode(arena, p, key_len);
            pairs[idx].value = url_decode(arena, eq + 1, val_len);
        } else {
            pairs[idx].key = url_decode(arena, p, seg_len);
            pairs[idx].value = "";
        }
        idx++;

        if (amp) {
            p = amp + 1;
        } else {
            break;
        }
    }

    *out = pairs;
    *out_count = idx;
    return 0;
}
