/**
 * @file oas_error.h
 * @brief Error accumulation with JSON Pointer paths.
 *
 * Validator collects all errors, not just the first. Errors are
 * arena-allocated and freed together with the document.
 */

#ifndef LIBOAS_OAS_ERROR_H
#define LIBOAS_OAS_ERROR_H

#include <liboas/oas_alloc.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum : uint8_t {
    OAS_ERR_NONE = 0,
    OAS_ERR_PARSE,      /**< JSON/YAML parse error */
    OAS_ERR_SCHEMA,     /**< JSON Schema validation error */
    OAS_ERR_REF,        /**< $ref resolution error */
    OAS_ERR_TYPE,       /**< type mismatch */
    OAS_ERR_CONSTRAINT, /**< min/max/pattern violation */
    OAS_ERR_REQUIRED,   /**< missing required field */
    OAS_ERR_FORMAT,     /**< format validation failure */
    OAS_ERR_ALLOC,      /**< memory allocation failure */
} oas_error_kind_t;

typedef struct {
    oas_error_kind_t kind;
    const char *message; /**< arena-allocated */
    const char *path;    /**< JSON Pointer to error location */
    uint32_t line;       /**< source line (for parse errors) */
    uint32_t column;     /**< source column */
} oas_error_t;

typedef struct oas_error_list oas_error_list_t;

/**
 * @brief Create a new error list backed by an arena.
 * @param arena Arena for all error allocations.
 * @return Error list, or nullptr on failure.
 */
[[nodiscard]] oas_error_list_t *oas_error_list_create(oas_arena_t *arena);

/**
 * @brief Add an error to the list.
 * @param list Error list.
 * @param kind Error category.
 * @param path JSON Pointer path (will be copied into arena).
 * @param fmt  Printf-style format string for message.
 */
void oas_error_list_add(oas_error_list_t *list, oas_error_kind_t kind, const char *path,
                        const char *fmt, ...) __attribute__((format(printf, 4, 5)));

/**
 * @brief Get number of errors in the list.
 */
size_t oas_error_list_count(const oas_error_list_t *list);

/**
 * @brief Get error at index.
 * @return Error pointer, or nullptr if index out of range.
 */
const oas_error_t *oas_error_list_get(const oas_error_list_t *list, size_t index);

/**
 * @brief Check if list has any errors.
 */
bool oas_error_list_has_errors(const oas_error_list_t *list);

/**
 * @brief Get human-readable name for error kind.
 */
const char *oas_error_kind_name(oas_error_kind_t kind);

#endif /* LIBOAS_OAS_ERROR_H */
