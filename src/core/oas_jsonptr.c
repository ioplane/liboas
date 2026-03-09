#include "oas_jsonptr.h"

#include <errno.h>
#include <string.h>

yyjson_val *oas_jsonptr_resolve(yyjson_val *root, const char *pointer)
{
    if (!root || !pointer) {
        return nullptr;
    }
    /* Empty string -> whole document (RFC 6901 S5) */
    if (pointer[0] == '\0') {
        return root;
    }
    return yyjson_ptr_get(root, pointer);
}

yyjson_val *oas_jsonptr_resolve_ex(yyjson_val *root, const char *pointer, oas_error_list_t *errors)
{
    if (!root || !pointer) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, "", "JSON Pointer resolution: null %s",
                               !root ? "root" : "pointer");
        }
        return nullptr;
    }
    if (pointer[0] == '\0') {
        return root;
    }

    size_t len = strlen(pointer);
    yyjson_ptr_err err = {0};
    yyjson_val *val = yyjson_ptr_getx(root, pointer, len, &err);

    if (!val && errors) {
        oas_error_list_add(errors, OAS_ERR_REF, pointer, "JSON Pointer cannot be resolved: %s",
                           err.msg ? err.msg : "unknown error");
    }
    return val;
}

const char *oas_jsonptr_from_ref(const char *ref)
{
    if (!ref) {
        return nullptr;
    }
    if (ref[0] == '#') {
        return ref + 1;
    }
    return ref;
}

/* Unescape a single segment in-place: ~1 -> '/', ~0 -> '~' (RFC 6901 S4) */
static void unescape_segment(char *seg)
{
    char *dst = seg;
    const char *src = seg;
    while (*src) {
        if (src[0] == '~' && src[1] == '1') {
            *dst++ = '/';
            src += 2;
        } else if (src[0] == '~' && src[1] == '0') {
            *dst++ = '~';
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

int oas_jsonptr_parse(const char *pointer, char ***segments, size_t *count, oas_arena_t *arena)
{
    if (!pointer || !segments || !count || !arena) {
        return -EINVAL;
    }

    /* Empty pointer -> zero segments */
    if (pointer[0] == '\0') {
        *segments = nullptr;
        *count = 0;
        return 0;
    }

    /* Must start with '/' per RFC 6901 S3 */
    const char *p = pointer;
    if (*p != '/') {
        return -EINVAL;
    }
    p++; /* skip leading '/' */

    /* Count segments (number of '/' separators + 1) */
    size_t n = 1;
    for (const char *s = p; *s; s++) {
        if (*s == '/') {
            n++;
        }
    }

    char **segs = oas_arena_alloc(arena, sizeof(*segs) * n, _Alignof(char *));
    if (!segs) {
        return -ENOMEM;
    }

    /* Copy and split */
    size_t ptr_len = strlen(p);
    char *buf = oas_arena_alloc(arena, ptr_len + 1, 1);
    if (!buf) {
        return -ENOMEM;
    }
    memcpy(buf, p, ptr_len + 1);

    size_t idx = 0;
    char *tok = buf;
    for (;;) {
        char *slash = strchr(tok, '/');
        if (slash) {
            *slash = '\0';
        }
        unescape_segment(tok);
        segs[idx++] = tok;
        if (!slash) {
            break;
        }
        tok = slash + 1;
    }

    *segments = segs;
    *count = idx;
    return 0;
}
