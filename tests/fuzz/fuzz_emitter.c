#include <liboas/oas_alloc.h>
#include <liboas/oas_emitter.h>
#include <liboas/oas_parser.h>

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    oas_arena_t *arena = oas_arena_create(0);
    if (!arena) {
        return 0;
    }

    oas_doc_t *doc = oas_doc_parse(arena, (const char *)data, size, nullptr);
    if (doc) {
        char *json = oas_doc_emit_json(doc, nullptr, nullptr);
        oas_emit_free(json);
    }

    oas_arena_destroy(arena);
    return 0;
}
