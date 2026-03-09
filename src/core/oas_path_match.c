#include "core/oas_path_match.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

typedef enum : uint8_t {
    PATH_SEG_STATIC = 0,
    PATH_SEG_PARAM,
} path_seg_kind_t;

typedef struct {
    path_seg_kind_t kind;
    const char *text; /* segment text (static) or param name (without braces) */
    size_t text_len;
} path_segment_t;

typedef struct {
    const char *template_path;
    path_segment_t *segments;
    size_t segment_count;
    size_t static_count;
} path_template_t;

struct oas_path_matcher {
    path_template_t *templates;
    size_t count;
};

constexpr size_t MAX_SEGMENTS = 64;

/* Count segments in a path (between slashes, skipping leading slash) */
static size_t count_segments(const char *path, size_t len)
{
    if (len == 0) {
        return 0;
    }

    size_t count = 0;
    size_t i = 0;

    /* Skip leading slash */
    if (path[0] == '/') {
        i = 1;
    }

    /* Root path has zero segments */
    if (i >= len) {
        return 0;
    }

    size_t seg_start = i;
    while (i <= len) {
        if (i == len || path[i] == '/') {
            if (i > seg_start) {
                count++;
            }
            if (i < len && i + 1 < len) {
                seg_start = i + 1;
            } else {
                break;
            }
        }
        i++;
    }

    return count;
}

/* Parse a single template into segments, returns -EINVAL on empty segment */
static int parse_template(oas_arena_t *arena, const char *tmpl, path_template_t *out)
{
    size_t len = strlen(tmpl);
    out->template_path = tmpl;

    /* Handle root path "/" */
    if (len == 1 && tmpl[0] == '/') {
        out->segments = nullptr;
        out->segment_count = 0;
        out->static_count = 0;
        return 0;
    }

    size_t nseg = count_segments(tmpl, len);
    if (nseg == 0) {
        out->segments = nullptr;
        out->segment_count = 0;
        out->static_count = 0;
        return 0;
    }

    out->segments = oas_arena_alloc(arena, nseg * sizeof(*out->segments), _Alignof(path_segment_t));
    if (!out->segments) {
        return -ENOMEM;
    }

    out->segment_count = 0;
    out->static_count = 0;

    size_t i = (tmpl[0] == '/') ? 1 : 0;
    while (i < len) {
        /* Find end of segment */
        size_t seg_start = i;
        while (i < len && tmpl[i] != '/') {
            i++;
        }

        size_t seg_len = i - seg_start;

        /* Reject empty segments (double slashes) */
        if (seg_len == 0) {
            return -EINVAL;
        }

        path_segment_t *seg = &out->segments[out->segment_count];

        if (tmpl[seg_start] == '{' && tmpl[seg_start + seg_len - 1] == '}') {
            seg->kind = PATH_SEG_PARAM;
            /* Strip braces: name is between { and } */
            size_t name_len = seg_len - 2;
            char *name = oas_arena_alloc(arena, name_len + 1, 1);
            if (!name) {
                return -ENOMEM;
            }
            memcpy(name, &tmpl[seg_start + 1], name_len);
            name[name_len] = '\0';
            seg->text = name;
            seg->text_len = name_len;
        } else {
            seg->kind = PATH_SEG_STATIC;
            /* Copy segment text */
            char *text = oas_arena_alloc(arena, seg_len + 1, 1);
            if (!text) {
                return -ENOMEM;
            }
            memcpy(text, &tmpl[seg_start], seg_len);
            text[seg_len] = '\0';
            seg->text = text;
            seg->text_len = seg_len;
            out->static_count++;
        }

        out->segment_count++;

        /* Skip the slash */
        if (i < len) {
            i++;
        }
    }

    return 0;
}

oas_path_matcher_t *oas_path_matcher_create(oas_arena_t *arena, const char **templates,
                                            size_t count)
{
    if (!arena || !templates || count == 0) {
        return nullptr;
    }

    oas_path_matcher_t *matcher =
        oas_arena_alloc(arena, sizeof(*matcher), _Alignof(oas_path_matcher_t));
    if (!matcher) {
        return nullptr;
    }

    matcher->templates =
        oas_arena_alloc(arena, count * sizeof(*matcher->templates), _Alignof(path_template_t));
    if (!matcher->templates) {
        return nullptr;
    }

    matcher->count = 0;

    for (size_t i = 0; i < count; i++) {
        if (!templates[i]) {
            return nullptr;
        }

        int rc = parse_template(arena, templates[i], &matcher->templates[matcher->count]);
        if (rc < 0) {
            return nullptr;
        }

        matcher->count++;
    }

    return matcher;
}

/* Split a request path into segments, returning count. Segments stored in caller buffer. */
static int split_request_path(const char *path, size_t path_len, const char **seg_starts,
                              size_t *seg_lens, size_t max_segs, size_t *out_count)
{
    *out_count = 0;

    /* Handle root path */
    if (path_len == 1 && path[0] == '/') {
        return 0;
    }

    /* Strip trailing slash for normalization */
    size_t effective_len = path_len;
    if (effective_len > 1 && path[effective_len - 1] == '/') {
        effective_len--;
    }

    size_t i = (path[0] == '/') ? 1 : 0;

    while (i < effective_len) {
        if (*out_count >= max_segs) {
            return -EINVAL;
        }

        size_t seg_start = i;
        while (i < effective_len && path[i] != '/') {
            i++;
        }

        size_t seg_len = i - seg_start;

        /* Reject empty segments (double slashes) */
        if (seg_len == 0) {
            return -EINVAL;
        }

        seg_starts[*out_count] = &path[seg_start];
        seg_lens[*out_count] = seg_len;
        (*out_count)++;

        if (i < effective_len) {
            i++;
        }
    }

    return 0;
}

/* Try matching a single template against request segments */
static bool try_match(const path_template_t *tmpl, const char **seg_starts, const size_t *seg_lens,
                      size_t seg_count)
{
    if (tmpl->segment_count != seg_count) {
        return false;
    }

    for (size_t i = 0; i < seg_count; i++) {
        const path_segment_t *ts = &tmpl->segments[i];

        if (ts->kind == PATH_SEG_STATIC) {
            if (ts->text_len != seg_lens[i]) {
                return false;
            }
            if (memcmp(ts->text, seg_starts[i], seg_lens[i]) != 0) {
                return false;
            }
        } else {
            /* Param segment: any non-empty value matches (already guaranteed non-empty) */
        }
    }

    return true;
}

int oas_path_match(const oas_path_matcher_t *matcher, const char *request_path,
                   oas_path_match_result_t *result, oas_arena_t *arena)
{
    if (!matcher || !request_path || !result || !arena) {
        return -EINVAL;
    }

    /* Initialize result as not matched */
    result->matched = false;
    result->template_path = nullptr;
    result->params = nullptr;
    result->params_count = 0;

    size_t path_len = strlen(request_path);
    if (path_len == 0 || request_path[0] != '/') {
        return -EINVAL;
    }

    const char *seg_starts[MAX_SEGMENTS];
    size_t seg_lens[MAX_SEGMENTS];
    size_t seg_count = 0;

    int rc =
        split_request_path(request_path, path_len, seg_starts, seg_lens, MAX_SEGMENTS, &seg_count);
    if (rc < 0) {
        /* Invalid path (e.g., empty segments) — not an error, just no match */
        return 0;
    }

    /* Find best match: prefer more static segments */
    const path_template_t *best = nullptr;

    for (size_t i = 0; i < matcher->count; i++) {
        const path_template_t *tmpl = &matcher->templates[i];
        if (try_match(tmpl, seg_starts, seg_lens, seg_count)) {
            if (!best || tmpl->static_count > best->static_count) {
                best = tmpl;
            }
        }
    }

    if (!best) {
        return 0;
    }

    /* Populate result */
    result->matched = true;
    result->template_path = best->template_path;

    /* Count params */
    size_t param_count = 0;
    for (size_t i = 0; i < best->segment_count; i++) {
        if (best->segments[i].kind == PATH_SEG_PARAM) {
            param_count++;
        }
    }

    result->params_count = param_count;

    if (param_count == 0) {
        result->params = nullptr;
        return 0;
    }

    result->params =
        oas_arena_alloc(arena, param_count * sizeof(*result->params), _Alignof(oas_path_param_t));
    if (!result->params) {
        return -ENOMEM;
    }

    size_t pi = 0;
    for (size_t i = 0; i < best->segment_count; i++) {
        if (best->segments[i].kind != PATH_SEG_PARAM) {
            continue;
        }

        result->params[pi].name = best->segments[i].text;

        /* Copy value from the request path segment */
        char *value = oas_arena_alloc(arena, seg_lens[i] + 1, 1);
        if (!value) {
            return -ENOMEM;
        }
        memcpy(value, seg_starts[i], seg_lens[i]);
        value[seg_lens[i]] = '\0';
        result->params[pi].value = value;

        pi++;
    }

    return 0;
}
