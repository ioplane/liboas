#include <liboas/oas_alloc.h>
#include <liboas/oas_parser.h>

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

    /* oas_doc_parse() calls oas_ref_resolve_all() internally */
    oas_doc_t *doc = oas_doc_parse(arena, (const char *)data, size, nullptr);
    oas_doc_free(doc);
    oas_arena_destroy(arena);
    return 0;
}
