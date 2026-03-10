#include "oas_ref_cache.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** Default maximum number of cached documents */
constexpr int OAS_REF_CACHE_DEFAULT_MAX = 100;

typedef struct {
    char *uri;        /* owned copy of the URI key */
    yyjson_doc *doc;  /* owned yyjson document */
    yyjson_val *root; /* root value within doc */
    bool occupied;
} ref_cache_entry_t;

struct oas_ref_cache {
    ref_cache_entry_t *entries;
    size_t capacity; /* hash table capacity (power of 2 >= 2 * max) */
    size_t count;
    size_t max_documents;
};

/* FNV-1a hash for null-terminated string */
static size_t fnv1a(const char *str)
{
    size_t hash = 14695981039346656037ULL;
    for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
        hash ^= *p;
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* Round up to next power of 2 (minimum 16) */
static size_t next_pow2(size_t v)
{
    size_t n = 16;
    while (n < v) {
        if (n > SIZE_MAX / 2) {
            return n;
        }
        n *= 2;
    }
    return n;
}

oas_ref_cache_t *oas_ref_cache_create(int max_documents)
{
    if (max_documents <= 0) {
        max_documents = OAS_REF_CACHE_DEFAULT_MAX;
    }

    oas_ref_cache_t *cache = calloc(1, sizeof(*cache));
    if (!cache) {
        return nullptr;
    }

    /* Keep load factor <= 0.5 for good probe performance */
    cache->capacity = next_pow2((size_t)max_documents * 2);
    cache->max_documents = (size_t)max_documents;
    cache->count = 0;

    cache->entries = calloc(cache->capacity, sizeof(*cache->entries));
    if (!cache->entries) {
        free(cache);
        return nullptr;
    }

    return cache;
}

yyjson_val *oas_ref_cache_get(oas_ref_cache_t *cache, const char *uri)
{
    if (!cache || !uri) {
        return nullptr;
    }

    size_t mask = cache->capacity - 1;
    size_t idx = fnv1a(uri) & mask;

    for (size_t i = 0; i < cache->capacity; i++) {
        size_t slot = (idx + i) & mask;
        ref_cache_entry_t *entry = &cache->entries[slot];

        if (!entry->occupied) {
            return nullptr;
        }

        if (strcmp(entry->uri, uri) == 0) {
            return entry->root;
        }
    }

    return nullptr;
}

int oas_ref_cache_put(oas_ref_cache_t *cache, const char *uri, yyjson_doc *doc, yyjson_val *root)
{
    if (!cache || !uri || !doc || !root) {
        return -EINVAL;
    }

    /* Dedup: if URI already cached, free the caller's doc to uphold
     * the ownership contract (cache always takes ownership on return 0). */
    if (oas_ref_cache_get(cache, uri)) {
        yyjson_doc_free(doc);
        return 0;
    }

    if (cache->count >= cache->max_documents) {
        return -ENOSPC;
    }

    char *uri_copy = strdup(uri);
    if (!uri_copy) {
        return -ENOMEM;
    }

    size_t mask = cache->capacity - 1;
    size_t idx = fnv1a(uri) & mask;

    for (size_t i = 0; i < cache->capacity; i++) {
        size_t slot = (idx + i) & mask;
        ref_cache_entry_t *entry = &cache->entries[slot];

        if (!entry->occupied) {
            entry->uri = uri_copy;
            entry->doc = doc;
            entry->root = root;
            entry->occupied = true;
            cache->count++;
            return 0;
        }
    }

    /* Should never reach here given load factor constraints */
    free(uri_copy);
    return -ENOSPC;
}

size_t oas_ref_cache_count(const oas_ref_cache_t *cache)
{
    if (!cache) {
        return 0;
    }
    return cache->count;
}

void oas_ref_cache_destroy(oas_ref_cache_t *cache)
{
    if (!cache) {
        return;
    }

    for (size_t i = 0; i < cache->capacity; i++) {
        ref_cache_entry_t *entry = &cache->entries[i];
        if (entry->occupied) {
            free(entry->uri);
            yyjson_doc_free(entry->doc);
        }
    }

    free(cache->entries);
    free(cache);
}
