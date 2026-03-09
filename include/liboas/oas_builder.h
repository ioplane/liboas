/**
 * @file oas_builder.h
 * @brief Code-first builder API for OpenAPI 3.2 documents.
 *
 * Provides a fluent API for constructing OpenAPI specs programmatically.
 * All allocations use the arena allocator — single oas_arena_destroy() frees everything.
 *
 * Usage:
 * @code
 *   oas_arena_t *arena = oas_arena_create(0);
 *   oas_doc_t *doc = oas_doc_build(arena, "Pet Store", "1.0.0");
 *
 *   oas_schema_t *pet = oas_schema_build_object(arena);
 *   oas_schema_add_property(arena, pet, "id", oas_schema_build_int64(arena));
 *   oas_schema_add_property(arena, pet, "name", oas_schema_build_string(arena));
 *   oas_schema_set_required(arena, pet, "id", "name", NULL);
 *
 *   oas_doc_add_component_schema(doc, arena, "Pet", pet);
 *   char *json = oas_doc_emit_json(doc, nullptr, nullptr);
 * @endcode
 */

#ifndef LIBOAS_OAS_BUILDER_H
#define LIBOAS_OAS_BUILDER_H

#include <liboas/oas_alloc.h>
#include <liboas/oas_doc.h>
#include <liboas/oas_schema.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── Schema builders ───────────────────────────────────────────────────── */

/**
 * @brief Build a string schema.
 * @return Schema with type_mask = OAS_TYPE_STRING.
 */
[[nodiscard]] oas_schema_t *oas_schema_build_string(oas_arena_t *arena);

/**
 * @brief Build an integer schema with format "int32".
 */
[[nodiscard]] oas_schema_t *oas_schema_build_int32(oas_arena_t *arena);

/**
 * @brief Build an integer schema with format "int64".
 */
[[nodiscard]] oas_schema_t *oas_schema_build_int64(oas_arena_t *arena);

/**
 * @brief Build a number schema (double).
 */
[[nodiscard]] oas_schema_t *oas_schema_build_number(oas_arena_t *arena);

/**
 * @brief Build a boolean schema.
 */
[[nodiscard]] oas_schema_t *oas_schema_build_bool(oas_arena_t *arena);

/** Options for constrained string schemas */
typedef struct {
    int64_t min_length; /**< -1 = unset */
    int64_t max_length; /**< -1 = unset */
    const char *pattern;
    const char *format;
    const char *description;
} oas_string_opts_t;

/**
 * @brief Build a string schema with constraints.
 */
[[nodiscard]] oas_schema_t *oas_schema_build_string_ex(oas_arena_t *arena,
                                                       const oas_string_opts_t *opts);

/** Options for constrained numeric schemas */
typedef struct {
    double minimum; /**< NAN = unset */
    double maximum; /**< NAN = unset */
    bool exclusive_min;
    bool exclusive_max;
    double multiple_of; /**< NAN = unset */
    const char *format; /**< "int32", "int64", "float", "double" */
    const char *description;
} oas_number_opts_t;

/**
 * @brief Build an integer schema with constraints.
 */
[[nodiscard]] oas_schema_t *oas_schema_build_integer_ex(oas_arena_t *arena,
                                                        const oas_number_opts_t *opts);

/**
 * @brief Build a number schema with constraints.
 */
[[nodiscard]] oas_schema_t *oas_schema_build_number_ex(oas_arena_t *arena,
                                                       const oas_number_opts_t *opts);

/**
 * @brief Build an array schema with items type.
 * @param items Schema for array elements.
 */
[[nodiscard]] oas_schema_t *oas_schema_build_array(oas_arena_t *arena, oas_schema_t *items);

/**
 * @brief Build an empty object schema (add properties with oas_schema_add_property).
 */
[[nodiscard]] oas_schema_t *oas_schema_build_object(oas_arena_t *arena);

/**
 * @brief Set required property names on an object schema.
 * @param arena Arena allocator.
 * @param schema Target schema.
 * @param ... NULL-terminated list of property name strings.
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_schema_set_required(oas_arena_t *arena, oas_schema_t *schema, ...);

/**
 * @brief Set description on a schema.
 * @return 0 on success, -EINVAL if schema is nullptr.
 */
[[nodiscard]] int oas_schema_set_description(oas_schema_t *schema, const char *description);

/**
 * @brief Set additionalProperties schema on an object schema.
 * @return 0 on success, -EINVAL if schema is nullptr.
 */
[[nodiscard]] int oas_schema_set_additional_properties(oas_schema_t *schema,
                                                       oas_schema_t *additional);

/* ── Document builder ──────────────────────────────────────────────────── */

/**
 * @brief Create a minimal OAS 3.2.0 document.
 * @param arena Arena allocator (owns all memory).
 * @param title API title (info.title).
 * @param version API version (info.version).
 * @return Document with openapi="3.2.0", info.title and info.version set.
 */
[[nodiscard]] oas_doc_t *oas_doc_build(oas_arena_t *arena, const char *title, const char *version);

/**
 * @brief Add a server entry.
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_doc_add_server(oas_doc_t *doc, oas_arena_t *arena, const char *url,
                                     const char *description);

/**
 * @brief Add a named schema to components.schemas.
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_doc_add_component_schema(oas_doc_t *doc, oas_arena_t *arena, const char *name,
                                               oas_schema_t *schema);

/* ── Operation builder types ───────────────────────────────────────────── */

/** Parameter builder (NULL-terminated array sentinel: .name = NULL) */
typedef struct {
    const char *name;
    const char *in; /**< "query", "path", "header", "cookie" */
    const char *description;
    bool required;
    oas_schema_t *schema;
} oas_param_builder_t;

/** Response builder (sentinel: .status = 0) */
typedef struct {
    int status;
    const char *description;
    const char *content_type; /**< default: "application/json" */
    oas_schema_t *schema;
} oas_response_builder_t;

/** Operation builder */
typedef struct {
    const char *summary;
    const char *description;
    const char *operation_id;
    const char *tag;
    oas_param_builder_t *params; /**< NULL-terminated array (sentinel: .name=NULL) */
    oas_schema_t *request_body;
    const char *request_content_type; /**< default: "application/json" */
    bool request_body_required;
    oas_response_builder_t *responses; /**< sentinel: .status=0 */
} oas_op_builder_t;

/**
 * @brief Add an operation to a path.
 * @param path  Path template (e.g. "/pets", "/pets/{id}").
 * @param method HTTP method (case-insensitive: "get", "post", etc.).
 * @param op Operation builder data.
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_doc_add_path_op(oas_doc_t *doc, oas_arena_t *arena, const char *path,
                                      const char *method, const oas_op_builder_t *op);

#endif /* LIBOAS_OAS_BUILDER_H */
