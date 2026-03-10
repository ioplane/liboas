/**
 * @file oas_parser.h
 * @brief Parse OpenAPI 3.2 JSON documents into oas_doc_t.
 */

#ifndef LIBOAS_OAS_PARSER_H
#define LIBOAS_OAS_PARSER_H

#include <liboas/oas_doc.h>
#include <liboas/oas_error.h>

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief User-provided fetch callback for remote $ref resolution.
 * @param ctx      User context pointer.
 * @param url      URL to fetch.
 * @param out_data Output: malloc-allocated response body (caller frees).
 * @param out_len  Output: response body length.
 * @return 0 on success, negative errno on error.
 */
typedef int (*oas_ref_fetch_fn)(void *ctx, const char *url, char **out_data, size_t *out_len);

/**
 * @brief Options for extended $ref resolution during parsing.
 */
typedef struct {
    bool allow_remote;      /**< Allow HTTP/HTTPS fetch (default: false) */
    bool allow_file;        /**< Allow file $ref (default: false) */
    oas_ref_fetch_fn fetch; /**< User fetch callback (overrides built-in) */
    void *fetch_ctx;        /**< User context for fetch callback */
    const char *base_dir;   /**< Base directory for file refs (nullptr = cwd) */
    int max_documents;      /**< Max cached documents (default: 100) */
    int fetch_timeout_ms;   /**< HTTP timeout (default: 30000) */
    size_t max_fetch_size;  /**< Max fetch response size (default: 10 MB) */
    int max_redirects;      /**< Max HTTP redirects (default: 5) */
} oas_ref_options_t;

/**
 * @brief Parse an OpenAPI 3.2 document from JSON string.
 * @param arena  Arena for all allocations.
 * @param json   JSON data.
 * @param len    Data length in bytes.
 * @param errors Error list (may be nullptr).
 * @return Parsed document, or nullptr on failure.
 */
[[nodiscard]] oas_doc_t *oas_doc_parse(oas_arena_t *arena, const char *json, size_t len,
                                       oas_error_list_t *errors);

/**
 * @brief Parse an OpenAPI 3.2 document with extended $ref resolution options.
 * @param arena     Arena for all allocations.
 * @param json      JSON data.
 * @param len       Data length in bytes.
 * @param ref_opts  Ref resolution options (nullptr = local-only defaults).
 * @param errors    Error list (may be nullptr).
 * @return Parsed document, or nullptr on failure.
 */
[[nodiscard]] oas_doc_t *oas_doc_parse_ex(oas_arena_t *arena, const char *json, size_t len,
                                          const oas_ref_options_t *ref_opts,
                                          oas_error_list_t *errors);

/**
 * @brief Parse an OpenAPI 3.2 document from a file.
 * @param arena  Arena for all allocations.
 * @param path   File path.
 * @param errors Error list (may be nullptr).
 * @return Parsed document, or nullptr on failure.
 */
[[nodiscard]] oas_doc_t *oas_doc_parse_file(oas_arena_t *arena, const char *path,
                                            oas_error_list_t *errors);

#endif /* LIBOAS_OAS_PARSER_H */
