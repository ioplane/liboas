#include "parser/oas_uri.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/* Duplicate a string (or substring) into the arena. */
static char *arena_strndup(oas_arena_t *arena, const char *s, size_t len)
{
    char *buf = oas_arena_alloc(arena, len + 1, 1);
    if (!buf) {
        return nullptr;
    }
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

static char *arena_strdup(oas_arena_t *arena, const char *s)
{
    return arena_strndup(arena, s, strlen(s));
}

/* Parse port number from string. Returns 0 on invalid/empty. */
static uint16_t parse_port(const char *s, size_t len)
{
    if (len == 0 || len > 5) {
        return 0;
    }

    unsigned long val = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
        val = val * 10 + (unsigned long)(s[i] - '0');
    }
    if (val > 65535) {
        return 0;
    }
    return (uint16_t)val;
}

int oas_uri_parse(const char *uri, oas_arena_t *arena, oas_uri_t *out)
{
    if (!uri || !arena || !out) {
        return -EINVAL;
    }

    memset(out, 0, sizeof(*out));
    const char *p = uri;

    /* Fragment-only reference: #... */
    if (*p == '#') {
        out->is_fragment_only = true;
        out->fragment = arena_strdup(arena, p + 1);
        if (!out->fragment) {
            return -ENOMEM;
        }
        return 0;
    }

    /* Check for scheme: RFC 3986 scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) */
    const char *colon = strchr(p, ':');
    const char *slash = strchr(p, '/');

    if (colon && (!slash || colon < slash)) {
        /* Verify scheme characters */
        bool valid_scheme = (p != colon);
        for (const char *c = p; c < colon && valid_scheme; c++) {
            if (c == p) {
                /* First char must be alpha */
                valid_scheme = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z');
            } else {
                valid_scheme = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
                               (*c >= '0' && *c <= '9') || *c == '+' || *c == '-' || *c == '.';
            }
        }

        if (valid_scheme) {
            out->scheme = arena_strndup(arena, p, (size_t)(colon - p));
            if (!out->scheme) {
                return -ENOMEM;
            }
            out->is_absolute = true;
            p = colon + 1;
        }
    }

    /* Authority: //host[:port] */
    if (p[0] == '/' && p[1] == '/') {
        p += 2;

        /* Find end of authority (next '/', '?', '#', or end) */
        const char *auth_end = p;
        while (*auth_end && *auth_end != '/' && *auth_end != '?' && *auth_end != '#') {
            auth_end++;
        }

        /* Check for port (last ':' in authority) — forward scan to avoid UB */
        const char *port_colon = nullptr;
        for (const char *c = p; c < auth_end; c++) {
            if (*c == ':') {
                port_colon = c;
            }
        }

        if (port_colon) {
            out->host = arena_strndup(arena, p, (size_t)(port_colon - p));
            if (!out->host) {
                return -ENOMEM;
            }
            out->port = parse_port(port_colon + 1, (size_t)(auth_end - port_colon - 1));
        } else {
            out->host = arena_strndup(arena, p, (size_t)(auth_end - p));
            if (!out->host) {
                return -ENOMEM;
            }
        }

        p = auth_end;
    }

    /* Path: up to '?' or '#' */
    if (*p && *p != '?' && *p != '#') {
        const char *path_end = p;
        while (*path_end && *path_end != '?' && *path_end != '#') {
            path_end++;
        }
        out->path = arena_strndup(arena, p, (size_t)(path_end - p));
        if (!out->path) {
            return -ENOMEM;
        }
        p = path_end;
    }

    /* Query: ?... up to '#' or end */
    if (*p == '?') {
        p++;
        const char *query_end = strchr(p, '#');
        if (query_end) {
            out->query = arena_strndup(arena, p, (size_t)(query_end - p));
            if (!out->query) {
                return -ENOMEM;
            }
            p = query_end;
        } else {
            out->query = arena_strdup(arena, p);
            if (!out->query) {
                return -ENOMEM;
            }
            p += strlen(p);
        }
    }

    /* Fragment: #... */
    if (*p == '#') {
        p++;
        out->fragment = arena_strdup(arena, p);
        if (!out->fragment) {
            return -ENOMEM;
        }
    }

    return 0;
}

/*
 * Remove last segment from a path to get the "merge base" directory.
 * "/a/b/c" -> "/a/b/"
 * "/a" -> "/"
 */
static const char *merge_base(oas_arena_t *arena, const char *path)
{
    if (!path || !*path) {
        return "/";
    }

    const char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        return "";
    }

    size_t len = (size_t)(last_slash - path) + 1;
    return arena_strndup(arena, path, len);
}

int oas_uri_resolve(const oas_uri_t *base, const char *ref, oas_arena_t *arena, char **out)
{
    if (!base || !ref || !arena || !out) {
        return -EINVAL;
    }

    oas_uri_t parsed;
    int rc = oas_uri_parse(ref, arena, &parsed);
    if (rc < 0) {
        return rc;
    }

    /* Absolute URI passes through unchanged */
    if (parsed.is_absolute) {
        *out = arena_strdup(arena, ref);
        return *out ? 0 : -ENOMEM;
    }

    /* Fragment-only: use base URI + new fragment */
    if (parsed.is_fragment_only) {
        /* Reconstruct base URI with new fragment */
        size_t len = 0;
        if (base->scheme) {
            len += strlen(base->scheme) + 3; /* scheme:// */
        }
        if (base->host) {
            len += strlen(base->host);
        }
        if (base->port > 0) {
            len += 6; /* :65535 */
        }
        if (base->path) {
            len += strlen(base->path);
        }
        if (parsed.fragment) {
            len += strlen(parsed.fragment) + 1; /* #fragment */
        }
        len += 1; /* null terminator */

        char *buf = oas_arena_alloc(arena, len, 1);
        if (!buf) {
            return -ENOMEM;
        }

        char *p = buf;
        if (base->scheme) {
            p += sprintf(p, "%s://", base->scheme);
        }
        if (base->host) {
            p += sprintf(p, "%s", base->host);
        }
        if (base->port > 0) {
            p += sprintf(p, ":%u", (unsigned)base->port);
        }
        if (base->path) {
            p += sprintf(p, "%s", base->path);
        }
        if (parsed.fragment) {
            (void)sprintf(p, "#%s", parsed.fragment);
        }
        *out = buf;
        return 0;
    }

    /* Relative path: merge with base path directory */
    const char *base_dir = merge_base(arena, base->path);
    if (!base_dir) {
        return -ENOMEM;
    }
    const char *rel_path = parsed.path ? parsed.path : "";

    size_t len = 0;
    if (base->scheme) {
        len += strlen(base->scheme) + 3;
    }
    if (base->host) {
        len += strlen(base->host);
    }
    if (base->port > 0) {
        len += 6;
    }
    len += strlen(base_dir) + strlen(rel_path);
    if (parsed.fragment) {
        len += strlen(parsed.fragment) + 1;
    }
    if (parsed.query) {
        len += strlen(parsed.query) + 1;
    }
    len += 1;

    char *buf = oas_arena_alloc(arena, len, 1);
    if (!buf) {
        return -ENOMEM;
    }

    char *p = buf;
    if (base->scheme) {
        p += sprintf(p, "%s://", base->scheme);
    }
    if (base->host) {
        p += sprintf(p, "%s", base->host);
    }
    if (base->port > 0) {
        p += sprintf(p, ":%u", (unsigned)base->port);
    }
    p += sprintf(p, "%s%s", base_dir, rel_path);
    if (parsed.query) {
        p += sprintf(p, "?%s", parsed.query);
    }
    if (parsed.fragment) {
        (void)sprintf(p, "#%s", parsed.fragment);
    }

    *out = buf;
    return 0;
}

bool oas_uri_path_is_safe(const char *path)
{
    if (!path) {
        return true;
    }

    /* Reject any occurrence of ".." as a path segment */
    const char *p = path;
    while (*p) {
        /* Check for ".." at start or after '/' */
        if (p[0] == '.' && p[1] == '.') {
            /* ".." at start of path */
            if (p == path && (p[2] == '/' || p[2] == '\0')) {
                return false;
            }
            /* "/.." in path */
            if (p > path && p[-1] == '/' && (p[2] == '/' || p[2] == '\0')) {
                return false;
            }
        }
        p++;
    }

    return true;
}
