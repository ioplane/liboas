#include "core/oas_path_match.h"

#include <liboas/oas_alloc.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char *TEMPLATES[] = {
    "/pets",
    "/pets/{petId}",
    "/users/{userId}/posts/{postId}",
    "/api/v1/items",
};
static constexpr size_t TEMPLATE_COUNT = 4;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char *path = malloc(size + 1);
    if (!path) {
        return 0;
    }
    memcpy(path, data, size);
    path[size] = '\0';

    oas_arena_t *arena = oas_arena_create(0);
    if (!arena) {
        free(path);
        return 0;
    }

    oas_path_matcher_t *matcher = oas_path_matcher_create(arena, TEMPLATES, TEMPLATE_COUNT);
    if (matcher) {
        oas_path_match_result_t result = {0};
        (void)oas_path_match(matcher, path, &result, arena);
    }

    oas_arena_destroy(arena);
    free(path);
    return 0;
}
