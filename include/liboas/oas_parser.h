/**
 * @file oas_parser.h
 * @brief Parse OpenAPI 3.2 JSON documents into oas_doc_t.
 */

#ifndef LIBOAS_OAS_PARSER_H
#define LIBOAS_OAS_PARSER_H

#include <liboas/oas_doc.h>
#include <liboas/oas_error.h>

#include <stddef.h>

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
 * @brief Parse an OpenAPI 3.2 document from a file.
 * @param arena  Arena for all allocations.
 * @param path   File path.
 * @param errors Error list (may be nullptr).
 * @return Parsed document, or nullptr on failure.
 */
[[nodiscard]] oas_doc_t *oas_doc_parse_file(oas_arena_t *arena, const char *path,
                                            oas_error_list_t *errors);

#endif /* LIBOAS_OAS_PARSER_H */
