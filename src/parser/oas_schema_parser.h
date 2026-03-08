/**
 * @file oas_schema_parser.h
 * @brief Parse yyjson value into oas_schema_t tree.
 */

#ifndef LIBOAS_PARSER_OAS_SCHEMA_PARSER_H
#define LIBOAS_PARSER_OAS_SCHEMA_PARSER_H

#include <liboas/oas_schema.h>

#include <yyjson.h>

/**
 * @brief Parse a JSON Schema node from a yyjson value.
 * @param arena  Arena for all allocations.
 * @param val    yyjson value representing a JSON Schema object.
 * @param errors Error list to append parse errors to (may be nullptr).
 * @return Parsed schema tree, or nullptr on failure.
 */
[[nodiscard]] oas_schema_t *oas_schema_parse(oas_arena_t *arena, yyjson_val *val,
                                             oas_error_list_t *errors);

#endif /* LIBOAS_PARSER_OAS_SCHEMA_PARSER_H */
