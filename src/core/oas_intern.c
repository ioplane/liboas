#include "oas_intern.h"

#include <liboas/oas_alloc.h>

#include <string.h>

/** Default hash table capacity */
constexpr size_t OAS_INTERN_DEFAULT_CAP = 256;

typedef struct oas_intern_entry {
    const char *str;
    size_t len;
    struct oas_intern_entry *next; /* chaining for collisions */
} oas_intern_entry_t;

struct oas_intern {
    oas_arena_t *arena;
    oas_intern_entry_t **buckets;
    size_t capacity;
    size_t count;
};

/* FNV-1a hash */
static size_t fnv1a(const char *data, size_t len)
{
    size_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= (unsigned char)data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

oas_intern_t *oas_intern_create(oas_arena_t *arena, size_t capacity)
{
    if (!arena) {
        return nullptr;
    }

    if (capacity == 0) {
        capacity = OAS_INTERN_DEFAULT_CAP;
    }

    oas_intern_t *pool = oas_arena_alloc(arena, sizeof(*pool), _Alignof(oas_intern_t));
    if (!pool) {
        return nullptr;
    }

    size_t buckets_size = sizeof(oas_intern_entry_t *) * capacity;
    pool->buckets = oas_arena_alloc(arena, buckets_size, _Alignof(oas_intern_entry_t *));
    if (!pool->buckets) {
        return nullptr;
    }

    memset(pool->buckets, 0, buckets_size);
    pool->arena = arena;
    pool->capacity = capacity;
    pool->count = 0;
    return pool;
}

const char *oas_intern_get(oas_intern_t *pool, const char *str, size_t len)
{
    if (!pool || !str) {
        return nullptr;
    }

    size_t idx = fnv1a(str, len) % pool->capacity;

    /* Search existing entries in this bucket */
    for (oas_intern_entry_t *e = pool->buckets[idx]; e; e = e->next) {
        if (e->len == len && memcmp(e->str, str, len) == 0) {
            return e->str;
        }
    }

    /* Not found — allocate and insert */
    char *copy = oas_arena_alloc(pool->arena, len + 1, 1);
    if (!copy) {
        return nullptr;
    }
    memcpy(copy, str, len);
    copy[len] = '\0';

    oas_intern_entry_t *entry =
        oas_arena_alloc(pool->arena, sizeof(*entry), _Alignof(oas_intern_entry_t));
    if (!entry) {
        return nullptr;
    }

    entry->str = copy;
    entry->len = len;
    entry->next = pool->buckets[idx];
    pool->buckets[idx] = entry;
    pool->count++;

    return copy;
}

size_t oas_intern_count(const oas_intern_t *pool)
{
    if (!pool) {
        return 0;
    }
    return pool->count;
}
