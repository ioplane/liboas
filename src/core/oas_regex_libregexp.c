#include "oas_regex.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "libregexp.h"
#pragma GCC diagnostic pop

/* Compiled pattern wrapping libregexp bytecode */
struct oas_compiled_pattern {
    uint8_t *bytecode;
    int bytecode_len;
    int capture_count;
};

/* libregexp required callbacks */

bool lre_check_stack_overflow(void *opaque, size_t alloca_size)
{
    (void)opaque;
    (void)alloca_size;
    return false;
}

int lre_check_timeout(void *opaque)
{
    (void)opaque;
    return 0;
}

void *lre_realloc(void *opaque, void *ptr, size_t size)
{
    (void)opaque;
    if (size == 0) {
        free(ptr);
        return nullptr;
    }
    return realloc(ptr, size);
}

static int libregexp_compile(oas_regex_backend_t *backend, const char *pattern,
                             oas_compiled_pattern_t **out)
{
    (void)backend;

    if (!pattern || !out) {
        return -EINVAL;
    }

    *out = nullptr;

    char error_msg[128];
    int bytecode_len;

    /* LRE_FLAG_UNICODE for full Unicode property support */
    uint8_t *bytecode = lre_compile(&bytecode_len, error_msg, sizeof(error_msg), pattern,
                                    strlen(pattern), LRE_FLAG_UNICODE, nullptr);
    if (!bytecode) {
        return -EINVAL;
    }

    oas_compiled_pattern_t *compiled = malloc(sizeof(*compiled));
    if (!compiled) {
        free(bytecode);
        return -ENOMEM;
    }

    compiled->bytecode = bytecode;
    compiled->bytecode_len = bytecode_len;
    compiled->capture_count = lre_get_capture_count(bytecode);

    *out = compiled;
    return 0;
}

static bool libregexp_match(oas_regex_backend_t *backend, const oas_compiled_pattern_t *compiled,
                            const char *value, size_t len)
{
    (void)backend;

    if (!compiled || !value) {
        return false;
    }

    const uint8_t *input = (const uint8_t *)value;
    int input_len = (int)len;

    /* Allocate capture array: 2 entries per capture group (start + end) */
    int alloc_count = compiled->capture_count * 2;
    uint8_t **capture = calloc((size_t)alloc_count, sizeof(*capture));
    if (!capture) {
        return false;
    }

    /* Try matching at each position (unanchored per JSON Schema) */
    for (int i = 0; i <= input_len; i++) {
        int rc = lre_exec(capture, compiled->bytecode, input, i, input_len, 0, nullptr);
        if (rc == 1) {
            free(capture);
            return true;
        }
        if (rc < 0) {
            break;
        }
    }

    free(capture);
    return false;
}

static void libregexp_free_pattern(oas_regex_backend_t *backend, oas_compiled_pattern_t *compiled)
{
    (void)backend;

    if (!compiled) {
        return;
    }

    free(compiled->bytecode);
    free(compiled);
}

static void libregexp_destroy(oas_regex_backend_t *backend)
{
    free(backend);
}

oas_regex_backend_t *oas_regex_libregexp_create(void)
{
    oas_regex_backend_t *backend = malloc(sizeof(*backend));
    if (!backend) {
        return nullptr;
    }

    backend->compile = libregexp_compile;
    backend->match = libregexp_match;
    backend->free_pattern = libregexp_free_pattern;
    backend->destroy = libregexp_destroy;

    return backend;
}
