/**
 * @file oas_yaml.h
 * @brief YAML 1.2 parsing via libfyaml with conversion to yyjson document.
 *
 * Converts libfyaml document tree to an immutable yyjson_doc so the
 * existing JSON-based OAS parsers can consume YAML input transparently.
 * This module is only available when OAS_YAML is enabled at build time.
 */

#ifndef LIBOAS_PARSER_YAML_H
#define LIBOAS_PARSER_YAML_H

#include <stddef.h>

#include <yyjson.h>

/**
 * @brief Parse YAML string and convert to yyjson document.
 * @param yaml     YAML content string.
 * @param len      Length of YAML string.
 * @param err_msg  Buffer for error message (nullable).
 * @param err_size Size of error message buffer.
 * @return yyjson_doc on success, nullptr on failure.
 */
[[nodiscard]] yyjson_doc *oas_yaml_to_json(const char *yaml, size_t len, char *err_msg,
                                           size_t err_size);

/**
 * @brief Parse YAML file and convert to yyjson document.
 * @param filepath Path to YAML file.
 * @param err_msg  Buffer for error message (nullable).
 * @param err_size Size of error message buffer.
 * @return yyjson_doc on success, nullptr on failure.
 */
[[nodiscard]] yyjson_doc *oas_yaml_file_to_json(const char *filepath, char *err_msg,
                                                size_t err_size);

/**
 * @brief Auto-detect format (JSON or YAML) and parse to yyjson document.
 *
 * Tries JSON first (fast path), falls back to YAML if JSON parse fails.
 * @param content  Input content string.
 * @param len      Length of content.
 * @param err_msg  Buffer for error message (nullable).
 * @param err_size Size of error message buffer.
 * @return yyjson_doc on success, nullptr on failure.
 */
[[nodiscard]] yyjson_doc *oas_auto_parse(const char *content, size_t len, char *err_msg,
                                         size_t err_size);

#endif /* LIBOAS_PARSER_YAML_H */
