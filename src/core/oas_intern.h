/**
 * @file oas_intern.h
 * @brief String interning pool for repeated schema keys.
 *
 * Same string always returns the same pointer, enabling pointer
 * comparison instead of strcmp for hot-path lookups.
 */

#ifndef LIBOAS_CORE_OAS_INTERN_H
#define LIBOAS_CORE_OAS_INTERN_H

#include <liboas/oas_alloc.h>

#include <stddef.h>

typedef struct oas_intern oas_intern_t;

/**
 * @brief Create a string interning pool.
 * @param arena    Arena for all string storage and hash table.
 * @param capacity Initial hash table capacity (0 = default 256).
 * @return Intern pool, or nullptr on failure.
 */
[[nodiscard]] oas_intern_t *oas_intern_create(oas_arena_t *arena, size_t capacity);

/**
 * @brief Intern a string — returns canonical pointer.
 * @param pool Interning pool.
 * @param str  String data (not necessarily null-terminated).
 * @param len  Length in bytes.
 * @return Interned null-terminated string, or nullptr on failure.
 *
 * If the same string was previously interned, returns the same pointer.
 */
const char *oas_intern_get(oas_intern_t *pool, const char *str, size_t len);

/**
 * @brief Get number of unique strings interned.
 */
size_t oas_intern_count(const oas_intern_t *pool);

#endif /* LIBOAS_CORE_OAS_INTERN_H */
