/**
 * @file oas_cookie.h
 * @brief HTTP Cookie header parser.
 */

#ifndef LIBOAS_CORE_OAS_COOKIE_H
#define LIBOAS_CORE_OAS_COOKIE_H

#include <liboas/oas_alloc.h>

#include <stddef.h>

typedef struct {
    const char *name;
    const char *value;
} oas_cookie_t;

/**
 * @brief Parse an HTTP Cookie header into name-value pairs.
 * @param header  Raw Cookie header value (e.g., "a=1; b=2").
 * @param arena   Arena for allocations.
 * @param out     Receives array of cookie pairs.
 * @param count   Receives number of pairs.
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_cookie_parse(const char *header, oas_arena_t *arena, oas_cookie_t **out,
                                   size_t *count);

#endif /* LIBOAS_CORE_OAS_COOKIE_H */
