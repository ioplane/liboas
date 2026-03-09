#include "oas_regex.h"

#include <errno.h>
#include <stdlib.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

/* Compiled pattern wrapping a pcre2_code + optional JIT match context */
struct oas_compiled_pattern {
    pcre2_code *code;
    pcre2_match_data *match_data;
};

/* PCRE2 backend instance (stateless beyond vtable, but extensible) */
typedef struct {
    oas_regex_backend_t base;
} oas_regex_pcre2_t;

static int pcre2_backend_compile(oas_regex_backend_t *backend, const char *pattern,
                                 oas_compiled_pattern_t **out)
{
    (void)backend;

    if (!pattern || !out)
        return -EINVAL;

    *out = nullptr;

    int errcode;
    PCRE2_SIZE erroffset;

    /* PCRE2_UTF | PCRE2_UCP for Unicode, ALT_BSUX for ECMA-262 \uHHHH,
     * MATCH_UNSET_BACKREF for JS-compatible unset backreference behavior */
    uint32_t options =
        PCRE2_UTF | PCRE2_UCP | PCRE2_ALT_BSUX | PCRE2_MATCH_UNSET_BACKREF;

    pcre2_code *code = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
                                     options, &errcode, &erroffset, nullptr);
    if (!code)
        return -EINVAL;

    /* Attempt JIT compilation — failure is non-fatal */
    (void)pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);

    pcre2_match_data *match_data =
        pcre2_match_data_create_from_pattern(code, nullptr);
    if (!match_data) {
        pcre2_code_free(code);
        return -ENOMEM;
    }

    oas_compiled_pattern_t *compiled = malloc(sizeof(*compiled));
    if (!compiled) {
        pcre2_match_data_free(match_data);
        pcre2_code_free(code);
        return -ENOMEM;
    }

    compiled->code = code;
    compiled->match_data = match_data;
    *out = compiled;
    return 0;
}

static bool pcre2_backend_match(oas_regex_backend_t *backend,
                                const oas_compiled_pattern_t *compiled,
                                const char *value, size_t len)
{
    (void)backend;

    if (!compiled || !value)
        return false;

    int rc = pcre2_match(compiled->code, (PCRE2_SPTR)value, len, 0, 0,
                         compiled->match_data, nullptr);
    return rc >= 0;
}

static void pcre2_backend_free_pattern(oas_regex_backend_t *backend,
                                       oas_compiled_pattern_t *compiled)
{
    (void)backend;

    if (!compiled)
        return;

    pcre2_match_data_free(compiled->match_data);
    pcre2_code_free(compiled->code);
    free(compiled);
}

static void pcre2_backend_destroy(oas_regex_backend_t *backend)
{
    free(backend);
}

oas_regex_backend_t *oas_regex_pcre2_create(void)
{
    oas_regex_pcre2_t *pcre2 = malloc(sizeof(*pcre2));
    if (!pcre2)
        return nullptr;

    pcre2->base.compile = pcre2_backend_compile;
    pcre2->base.match = pcre2_backend_match;
    pcre2->base.free_pattern = pcre2_backend_free_pattern;
    pcre2->base.destroy = pcre2_backend_destroy;

    return &pcre2->base;
}
