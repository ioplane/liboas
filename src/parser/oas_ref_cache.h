/**
 * @file oas_ref_cache.h
 * @brief Document cache for external $ref resolution.
 *
 * URI-keyed cache that stores parsed yyjson documents and their root values.
 * Takes ownership of documents on successful put; frees them on destroy.
 */

#ifndef LIBOAS_PARSER_REF_CACHE_H
#define LIBOAS_PARSER_REF_CACHE_H

#include <stddef.h>

#include <yyjson.h>

typedef struct oas_ref_cache oas_ref_cache_t;

/**
 * @brief Create document cache.
 * @param max_documents  Maximum cached documents (0 = default 100)
 * @return Cache handle, or nullptr on allocation failure
 */
[[nodiscard]] oas_ref_cache_t *oas_ref_cache_create(int max_documents);

/**
 * @brief Look up cached document by URI (without fragment).
 * @return yyjson root value, or nullptr if not cached
 */
yyjson_val *oas_ref_cache_get(oas_ref_cache_t *cache, const char *uri);

/**
 * @brief Store parsed document in cache. Cache takes ownership of yyjson_doc.
 * @return 0 on success, -ENOMEM, -ENOSPC if cache full
 */
[[nodiscard]] int oas_ref_cache_put(oas_ref_cache_t *cache, const char *uri, yyjson_doc *doc,
                                    yyjson_val *root);

/**
 * @brief Get number of cached documents.
 */
size_t oas_ref_cache_count(const oas_ref_cache_t *cache);

/**
 * @brief Destroy cache and all owned yyjson_docs.
 */
void oas_ref_cache_destroy(oas_ref_cache_t *cache);

#endif /* LIBOAS_PARSER_REF_CACHE_H */
