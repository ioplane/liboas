#include <liboas/oas_error.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/** Initial capacity for the error array */
constexpr size_t OAS_ERROR_LIST_INIT_CAP = 16;

struct oas_error_list {
    oas_arena_t *arena;
    oas_error_t *errors;
    size_t count;
    size_t capacity;
};

oas_error_list_t *oas_error_list_create(oas_arena_t *arena)
{
    if (!arena) {
        return nullptr;
    }

    oas_error_list_t *list = oas_arena_alloc(arena, sizeof(*list), _Alignof(oas_error_list_t));
    if (!list) {
        return nullptr;
    }

    list->arena = arena;
    list->count = 0;
    list->capacity = OAS_ERROR_LIST_INIT_CAP;
    list->errors =
        oas_arena_alloc(arena, sizeof(oas_error_t) * list->capacity, _Alignof(oas_error_t));
    if (!list->errors) {
        return nullptr;
    }

    return list;
}

static char *arena_strdup(oas_arena_t *arena, const char *str)
{
    if (!str) {
        return nullptr;
    }
    size_t len = strlen(str) + 1;
    char *copy = oas_arena_alloc(arena, len, 1);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

static bool error_list_grow(oas_error_list_t *list)
{
    size_t new_cap = list->capacity * 2;
    oas_error_t *new_errors =
        oas_arena_alloc(list->arena, sizeof(oas_error_t) * new_cap, _Alignof(oas_error_t));
    if (!new_errors) {
        return false;
    }
    memcpy(new_errors, list->errors, sizeof(oas_error_t) * list->count);
    list->errors = new_errors;
    list->capacity = new_cap;
    return true;
}

void oas_error_list_add(oas_error_list_t *list, oas_error_kind_t kind, const char *path,
                        const char *fmt, ...)
{
    if (!list) {
        return;
    }

    /* Format message before grow — va_start/va_end must bracket tightly */
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    // codechecker_suppress [security.VAList] va_list is initialized by va_start above
    (void)vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (list->count >= list->capacity) {
        if (!error_list_grow(list)) {
            return;
        }
    }

    oas_error_t *err = &list->errors[list->count++];
    err->kind = kind;
    err->message = arena_strdup(list->arena, buf);
    err->path = arena_strdup(list->arena, path);
    err->line = 0;
    err->column = 0;
}

size_t oas_error_list_count(const oas_error_list_t *list)
{
    if (!list) {
        return 0;
    }
    return list->count;
}

const oas_error_t *oas_error_list_get(const oas_error_list_t *list, size_t index)
{
    if (!list || index >= list->count) {
        return nullptr;
    }
    return &list->errors[index];
}

bool oas_error_list_has_errors(const oas_error_list_t *list)
{
    if (!list) {
        return false;
    }
    return list->count > 0;
}

const char *oas_error_kind_name(oas_error_kind_t kind)
{
    switch (kind) {
    case OAS_ERR_NONE:
        return "none";
    case OAS_ERR_PARSE:
        return "parse";
    case OAS_ERR_SCHEMA:
        return "schema";
    case OAS_ERR_REF:
        return "ref";
    case OAS_ERR_TYPE:
        return "type";
    case OAS_ERR_CONSTRAINT:
        return "constraint";
    case OAS_ERR_REQUIRED:
        return "required";
    case OAS_ERR_FORMAT:
        return "format";
    case OAS_ERR_ALLOC:
        return "alloc";
    }
    return "unknown";
}
