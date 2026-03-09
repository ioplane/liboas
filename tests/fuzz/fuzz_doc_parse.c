#include <liboas/oas_alloc.h>
#include <liboas/oas_error.h>
#include <liboas/oas_parser.h>

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 131072) {
        return 0;
    }

    oas_arena_t *arena = oas_arena_create(0);
    if (!arena) {
        return 0;
    }

    /* Full doc parse pipeline with error collection */
    oas_error_list_t *errors = oas_error_list_create(arena);
    oas_doc_t *doc = oas_doc_parse(arena, (const char *)data, size, errors);
    oas_doc_free(doc);
    oas_arena_destroy(arena);
    return 0;
}
