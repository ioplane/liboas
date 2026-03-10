/**
 * @file oas_uri.h
 * @brief RFC 3986 URI parser for external $ref resolution.
 *
 * Parses URIs into components (scheme, host, port, path, query, fragment),
 * resolves relative references against a base URI, and validates path safety.
 */

#ifndef LIBOAS_PARSER_URI_H
#define LIBOAS_PARSER_URI_H

#include <liboas/oas_alloc.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *scheme;
    const char *host;
    uint16_t port;
    const char *path;
    const char *query;
    const char *fragment;
    bool is_absolute;
    bool is_fragment_only;
} oas_uri_t;

/**
 * @brief Parse a URI string into components per RFC 3986.
 * @param uri   URI string to parse.
 * @param arena Arena for string allocations.
 * @param out   Output URI structure.
 * @return 0 on success, -EINVAL on invalid input.
 */
[[nodiscard]] int oas_uri_parse(const char *uri, oas_arena_t *arena, oas_uri_t *out);

/**
 * @brief Resolve a relative URI reference against a base URI (RFC 3986 S5).
 * @param base  Parsed base URI.
 * @param ref   Reference URI string.
 * @param arena Arena for result allocation.
 * @param out   Output: resolved URI string.
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_uri_resolve(const oas_uri_t *base, const char *ref, oas_arena_t *arena,
                                  char **out);

/**
 * @brief Check if a URI path is safe (no directory traversal).
 * @param path Path string to check.
 * @return true if safe, false if path contains traversal sequences.
 */
bool oas_uri_path_is_safe(const char *path);

#endif /* LIBOAS_PARSER_URI_H */
