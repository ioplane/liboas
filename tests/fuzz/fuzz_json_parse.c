#include <liboas/oas_alloc.h>
#include <liboas/oas_parser.h>

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    oas_arena_t *arena = oas_arena_create(0);
    if (!arena) {
        return 0;
    }
    (void)oas_doc_parse(arena, (const char *)data, size, nullptr);
    oas_arena_destroy(arena);
    return 0;
}
