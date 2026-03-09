#include <liboas/oas_alloc.h>
#include <liboas/oas_compiler.h>
#include <liboas/oas_regex.h>
#include <liboas/oas_schema.h>

#include "parser/oas_schema_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <yyjson.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 65536) {
        return 0;
    }

    yyjson_doc *jdoc = yyjson_read((const char *)data, size, 0);
    if (!jdoc) {
        return 0;
    }

    oas_arena_t *arena = oas_arena_create(0);
    if (!arena) {
        yyjson_doc_free(jdoc);
        return 0;
    }

    yyjson_val *root = yyjson_doc_get_root(jdoc);
    oas_schema_t *schema = oas_schema_parse(arena, root, nullptr);
    if (!schema) {
        oas_arena_destroy(arena);
        yyjson_doc_free(jdoc);
        return 0;
    }

    oas_compiler_config_t config = {
        .regex = oas_regex_libregexp_create(),
        .format_policy = 0,
    };
    oas_compiled_schema_t *compiled = oas_schema_compile(schema, &config, nullptr);
    if (compiled) {
        oas_compiled_schema_free(compiled);
    } else if (config.regex) {
        config.regex->destroy(config.regex);
    }

    oas_arena_destroy(arena);
    yyjson_doc_free(jdoc);
    return 0;
}
