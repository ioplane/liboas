#include <liboas/oas_alloc.h>
#include <liboas/oas_compiler.h>
#include <liboas/oas_parser.h>
#include <liboas/oas_regex.h>

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 65536) {
        return 0;
    }

    oas_arena_t *arena = oas_arena_create(0);
    if (!arena) {
        return 0;
    }

    oas_doc_t *doc = oas_doc_parse(arena, (const char *)data, size, nullptr);
    if (!doc) {
        oas_arena_destroy(arena);
        return 0;
    }

    oas_compiler_config_t config = {
        .regex = oas_regex_libregexp_create(),
        .format_policy = 0,
    };
    oas_compiled_doc_t *compiled = oas_doc_compile(doc, &config, nullptr);
    if (compiled) {
        oas_compiled_doc_free(compiled);
    } else if (config.regex) {
        config.regex->destroy(config.regex);
    }

    oas_doc_free(doc);
    oas_arena_destroy(arena);
    return 0;
}
