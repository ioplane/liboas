#include <liboas/oas_negotiate.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *type;
    size_t type_len;
    const char *subtype;
    size_t subtype_len;
    double quality;
} accept_entry_t;

static void skip_ws(const char **p)
{
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
}

static double parse_quality(const char *params, size_t len)
{
    /* Search for q= parameter */
    const char *p = params;
    const char *end = params + len;
    while (p < end) {
        skip_ws(&p);
        if (p + 1 < end && *p == 'q' && *(p + 1) == '=') {
            p += 2;
            return strtod(p, nullptr);
        }
        /* Skip to next parameter */
        const char *next = memchr(p, ';', (size_t)(end - p));
        if (!next) {
            break;
        }
        p = next + 1;
    }
    return 1.0;
}

static bool parse_accept_entry(const char *start, size_t len, accept_entry_t *entry)
{
    const char *p = start;
    const char *end = start + len;

    skip_ws(&p);

    /* Find slash between type/subtype */
    const char *slash = memchr(p, '/', (size_t)(end - p));
    if (!slash) {
        return false;
    }

    entry->type = p;
    entry->type_len = (size_t)(slash - p);
    /* Trim trailing whitespace from type */
    while (entry->type_len > 0 && entry->type[entry->type_len - 1] == ' ') {
        entry->type_len--;
    }

    p = slash + 1;
    /* Find semicolon (parameters) or end */
    const char *semi = memchr(p, ';', (size_t)(end - p));
    if (semi) {
        entry->subtype = p;
        entry->subtype_len = (size_t)(semi - p);
        entry->quality = parse_quality(semi + 1, (size_t)(end - semi - 1));
    } else {
        entry->subtype = p;
        entry->subtype_len = (size_t)(end - p);
        entry->quality = 1.0;
    }
    /* Trim trailing whitespace from subtype */
    while (entry->subtype_len > 0 && entry->subtype[entry->subtype_len - 1] == ' ') {
        entry->subtype_len--;
    }

    return true;
}

static bool media_type_matches(const accept_entry_t *accept, const char *available)
{
    /* Wildcard type matches everything */
    if (accept->type_len == 1 && accept->type[0] == '*') {
        return true;
    }

    /* Find slash in available */
    const char *slash = strchr(available, '/');
    if (!slash) {
        return false;
    }

    size_t avail_type_len = (size_t)(slash - available);
    const char *avail_subtype = slash + 1;
    size_t avail_subtype_len = strlen(avail_subtype);

    /* Type must match */
    if (accept->type_len != avail_type_len ||
        strncasecmp(accept->type, available, avail_type_len) != 0) {
        return false;
    }

    /* Wildcard subtype */
    if (accept->subtype_len == 1 && accept->subtype[0] == '*') {
        return true;
    }

    /* Exact subtype match */
    return accept->subtype_len == avail_subtype_len &&
           strncasecmp(accept->subtype, avail_subtype, avail_subtype_len) == 0;
}

const char *oas_negotiate_content_type(const char *accept, const char **available, size_t count)
{
    if (!accept || !available || count == 0) {
        return nullptr;
    }

    const char *best_match = nullptr;
    double best_quality = -1.0;

    const char *p = accept;
    while (*p) {
        /* Find next comma or end */
        const char *comma = strchr(p, ',');
        size_t entry_len = comma ? (size_t)(comma - p) : strlen(p);

        accept_entry_t entry = {0};
        if (parse_accept_entry(p, entry_len, &entry)) {
            for (size_t i = 0; i < count; i++) {
                if (media_type_matches(&entry, available[i]) && entry.quality > best_quality) {
                    best_quality = entry.quality;
                    best_match = available[i];
                }
            }
        }

        if (comma) {
            p = comma + 1;
        } else {
            break;
        }
    }

    return best_match;
}
