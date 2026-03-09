/**
 * @file oas_query.h
 * @brief Query string parser.
 */

#ifndef LIBOAS_CORE_OAS_QUERY_H
#define LIBOAS_CORE_OAS_QUERY_H

#include <liboas/oas_alloc.h>

#include <stddef.h>

typedef struct {
    const char *key;
    const char *value;
} oas_query_pair_t;

/**
 * @brief Parse a URL query string into key-value pairs.
 * @param arena        Arena for allocations.
 * @param query_string Raw query string (without leading '?').
 * @param out          Receives array of key-value pairs.
 * @param out_count    Receives number of pairs.
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_query_parse(oas_arena_t *arena, const char *query_string,
                                  oas_query_pair_t **out, size_t *out_count);

#endif /* LIBOAS_CORE_OAS_QUERY_H */
