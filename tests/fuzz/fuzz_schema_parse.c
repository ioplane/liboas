#include <liboas/oas_alloc.h>
#include <liboas/oas_schema.h>

#include "parser/oas_schema_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <yyjson.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
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
    (void)oas_schema_parse(arena, root, nullptr);

    oas_arena_destroy(arena);
    yyjson_doc_free(jdoc);
    return 0;
}
