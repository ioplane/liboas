/**
 * @file oas_adapter.h
 * @brief OpenAPI adapter — unified spec-first and code-first validation facade.
 *
 * Wraps parser, compiler, validator, and emitter into a single lifecycle
 * for middleware integration. Supports both spec-first (load from file/string)
 * and code-first (from builder-constructed oas_doc_t) workflows.
 */

#ifndef LIBOAS_OAS_ADAPTER_H
#define LIBOAS_OAS_ADAPTER_H

#include <liboas/oas_doc.h>
#include <liboas/oas_error.h>
#include <liboas/oas_validator.h>

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool validate_requests;  /**< Validate incoming requests */
    bool validate_responses; /**< Validate outgoing responses (dev-mode) */
    bool serve_spec;         /**< Serve OpenAPI spec at spec_url */
    bool serve_scalar;       /**< Serve Scalar UI at docs_url */
    const char *spec_url;    /**< default: "/openapi.json" */
    const char *docs_url;    /**< default: "/docs" */
} oas_adapter_config_t;

typedef struct oas_adapter oas_adapter_t;

/**
 * @brief Create an adapter from a JSON spec string (spec-first).
 * @param json    OpenAPI JSON string.
 * @param len     Length of JSON string.
 * @param config  Adapter configuration (nullptr for defaults).
 * @param errors  Error list for diagnostics (nullable).
 * @return Adapter, or nullptr on failure.
 */
[[nodiscard]] oas_adapter_t *oas_adapter_create(const char *json, size_t len,
                                                const oas_adapter_config_t *config,
                                                oas_error_list_t *errors);

/**
 * @brief Create an adapter from a builder-constructed document (code-first).
 * @param doc     Document built via oas_doc_build / oas_doc_add_path_op.
 * @param arena   Arena that owns the document.
 * @param config  Adapter configuration (nullptr for defaults).
 * @param errors  Error list for diagnostics (nullable).
 * @return Adapter, or nullptr on failure.
 */
[[nodiscard]] oas_adapter_t *oas_adapter_from_doc(const oas_doc_t *doc, oas_arena_t *arena,
                                                  const oas_adapter_config_t *config,
                                                  oas_error_list_t *errors);

/**
 * @brief Destroy an adapter and free all resources.
 */
void oas_adapter_destroy(oas_adapter_t *adapter);

/**
 * @brief Get the parsed document from the adapter.
 */
const oas_doc_t *oas_adapter_doc(const oas_adapter_t *adapter);

/**
 * @brief Get the adapter configuration.
 */
const oas_adapter_config_t *oas_adapter_config(const oas_adapter_t *adapter);

/**
 * @brief Get the spec JSON string for serving.
 * @param adapter Adapter instance.
 * @param out_len Receives the JSON length (nullable).
 * @return Cached JSON string (owned by adapter), or nullptr if serve_spec is disabled.
 */
const char *oas_adapter_spec_json(const oas_adapter_t *adapter, size_t *out_len);

/**
 * @brief Validate an HTTP request using the adapter.
 * @return 0 on success, negative errno on error.
 */
[[nodiscard]] int oas_adapter_validate_request(const oas_adapter_t *adapter,
                                               const oas_http_request_t *req,
                                               oas_validation_result_t *result, oas_arena_t *arena);

/**
 * @brief Validate an HTTP response using the adapter.
 * @return 0 on success, negative errno on error.
 */
[[nodiscard]] int oas_adapter_validate_response(const oas_adapter_t *adapter, const char *path,
                                                const char *method, const oas_http_response_t *resp,
                                                oas_validation_result_t *result,
                                                oas_arena_t *arena);

/**
 * @brief Generate Scalar API documentation HTML.
 * @param title    Page title (nullable, defaults to "API Documentation").
 * @param spec_url URL to the OpenAPI spec JSON (e.g., "/openapi.json").
 * @param out_len  Receives the HTML length (nullable).
 * @return Heap-allocated HTML string. Caller must free().
 */
[[nodiscard]] char *oas_scalar_html(const char *title, const char *spec_url, size_t *out_len);

#endif /* LIBOAS_OAS_ADAPTER_H */
