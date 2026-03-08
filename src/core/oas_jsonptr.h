/**
 * @file oas_jsonptr.h
 * @brief JSON Pointer (RFC 6901) resolution and parsing.
 *
 * Thin wrapper around yyjson's built-in JSON Pointer support with
 * error list integration and segment parsing for $ref resolution.
 */

#ifndef LIBOAS_CORE_JSONPTR_H
#define LIBOAS_CORE_JSONPTR_H

#include <liboas/oas_alloc.h>
#include <liboas/oas_error.h>

#include <yyjson.h>

#include <stddef.h>

/**
 * @brief Resolve a JSON Pointer against a yyjson value tree.
 * @param root  Root value to resolve against.
 * @param pointer  JSON Pointer string (RFC 6901). Empty string returns root.
 * @return Resolved value, or nullptr if not found / invalid.
 */
yyjson_val *oas_jsonptr_resolve(yyjson_val *root, const char *pointer);

/**
 * @brief Resolve a JSON Pointer with error reporting.
 * @param root     Root value to resolve against.
 * @param pointer  JSON Pointer string (RFC 6901).
 * @param errors   Error list for diagnostics (may be nullptr).
 * @return Resolved value, or nullptr on failure.
 */
yyjson_val *oas_jsonptr_resolve_ex(yyjson_val *root, const char *pointer, oas_error_list_t *errors);

/**
 * @brief Extract JSON Pointer from a URI-style $ref fragment.
 *
 * "#/components/schemas/Pet" -> "/components/schemas/Pet"
 * "/already/pointer"         -> "/already/pointer"
 * "#"                        -> ""
 *
 * @param ref  Reference string (may start with '#').
 * @return Pointer into @p ref past the '#', or @p ref itself if no '#'.
 *         Returns nullptr if @p ref is nullptr.
 */
const char *oas_jsonptr_from_ref(const char *ref);

/**
 * @brief Parse a JSON Pointer into unescaped segments.
 *
 * Splits by '/' and applies RFC 6901 unescaping: ~1 -> '/', ~0 -> '~'.
 * Leading '/' is consumed (not stored as empty first segment).
 *
 * @param pointer   JSON Pointer string.
 * @param segments  Output: array of unescaped segment strings.
 * @param count     Output: number of segments.
 * @param arena     Arena for allocating segments.
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_jsonptr_parse(const char *pointer, char ***segments, size_t *count,
                                    oas_arena_t *arena);

#endif /* LIBOAS_CORE_JSONPTR_H */
