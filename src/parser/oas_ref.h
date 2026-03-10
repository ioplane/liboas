/**
 * @file oas_ref.h
 * @brief $ref resolver for OpenAPI documents.
 *
 * Resolves local JSON Pointer references (#/components/schemas/X) with
 * cycle detection. Walks the document model and links ref_resolved pointers.
 */

#ifndef LIBOAS_PARSER_REF_H
#define LIBOAS_PARSER_REF_H

#include <liboas/oas_alloc.h>
#include <liboas/oas_doc.h>
#include <liboas/oas_error.h>
#include <liboas/oas_schema.h>

#include <yyjson.h>

#include <stddef.h>

typedef struct oas_ref_ctx oas_ref_ctx_t;
typedef struct oas_ref_cache oas_ref_cache_t;

/**
 * @brief Create a $ref resolution context.
 * @param arena  Arena allocator for internal structures.
 * @param root   Root yyjson value of the document (for JSON Pointer resolution).
 * @return Context, or nullptr on failure.
 */
[[nodiscard]] oas_ref_ctx_t *oas_ref_ctx_create(oas_arena_t *arena, yyjson_val *root);

/**
 * @brief Resolve a single $ref string to a yyjson value.
 * @param ctx     Resolution context.
 * @param ref     $ref string (e.g. "#/components/schemas/Pet").
 * @param out     Output: resolved yyjson value.
 * @param errors  Error list for diagnostics.
 * @return 0 on success, -ENOENT if not found, -ELOOP on cycle.
 */
[[nodiscard]] int oas_ref_resolve(oas_ref_ctx_t *ctx, const char *ref, yyjson_val **out,
                                  oas_error_list_t *errors);

/**
 * @brief Resolve all $ref in component schemas of a parsed document.
 *
 * Walks components->schemas and resolves each schema's $ref to point at
 * the target schema's ref_resolved field.
 *
 * @param ctx     Resolution context.
 * @param doc     Parsed document to resolve.
 * @param errors  Error list for diagnostics.
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_ref_resolve_all(oas_ref_ctx_t *ctx, oas_doc_t *doc, oas_error_list_t *errors);

/**
 * @brief Load and parse a JSON/YAML file for $ref resolution.
 * @param path       File path (absolute or relative to base_dir)
 * @param base_dir   Base directory for relative paths (nullable = cwd)
 * @param cache      Document cache (stores result)
 * @param max_size   Maximum file size in bytes (0 = default 10 MB)
 * @param root_out   Output: yyjson root value
 * @param errors     Error accumulator
 * @return 0 on success, negative errno on error
 */
[[nodiscard]] int oas_ref_load_file(const char *path, const char *base_dir, oas_ref_cache_t *cache,
                                    size_t max_size, yyjson_val **root_out,
                                    oas_error_list_t *errors);

#endif /* LIBOAS_PARSER_REF_H */
