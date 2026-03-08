/**
 * @file oas_json.h
 * @brief yyjson integration wrapper with typed accessors.
 *
 * Thin wrapper around yyjson for consistent error handling
 * and typed access to JSON values.
 */

#ifndef LIBOAS_PARSER_OAS_JSON_H
#define LIBOAS_PARSER_OAS_JSON_H

#include <liboas/oas_error.h>

#include <yyjson.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    yyjson_doc *doc;  /**< yyjson document (owns memory) */
    yyjson_val *root; /**< root value */
} oas_json_doc_t;

/**
 * @brief Parse JSON from a memory buffer.
 * @param data   JSON data.
 * @param len    Data length in bytes.
 * @param out    Output document (caller-owned struct).
 * @param errors Error list to append parse errors to (may be nullptr).
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_json_parse(const char *data, size_t len, oas_json_doc_t *out,
                                 oas_error_list_t *errors);

/**
 * @brief Parse JSON from a file.
 * @param path   File path.
 * @param out    Output document.
 * @param errors Error list (may be nullptr).
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_json_parse_file(const char *path, oas_json_doc_t *out,
                                      oas_error_list_t *errors);

/**
 * @brief Free yyjson document resources.
 * @param doc Document to free (nullptr-safe).
 */
void oas_json_free(oas_json_doc_t *doc);

/**
 * @brief Get string value from object by key.
 * @return String pointer (yyjson-owned), or nullptr if missing/wrong type.
 */
[[nodiscard]] const char *oas_json_get_str(yyjson_val *obj, const char *key);

/**
 * @brief Get integer value from object by key.
 * @return Integer value, or def if missing/wrong type.
 */
[[nodiscard]] int64_t oas_json_get_int(yyjson_val *obj, const char *key, int64_t def);

/**
 * @brief Get boolean value from object by key.
 * @return Boolean value, or def if missing/wrong type.
 */
[[nodiscard]] bool oas_json_get_bool(yyjson_val *obj, const char *key, bool def);

/**
 * @brief Get object value from object by key.
 * @return Object value, or nullptr if missing/wrong type.
 */
[[nodiscard]] yyjson_val *oas_json_get_obj(yyjson_val *obj, const char *key);

/**
 * @brief Get array value from object by key.
 * @return Array value, or nullptr if missing/wrong type.
 */
[[nodiscard]] yyjson_val *oas_json_get_arr(yyjson_val *obj, const char *key);

#endif /* LIBOAS_PARSER_OAS_JSON_H */
