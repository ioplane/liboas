/**
 * @file oas_validator.h
 * @brief Validation VM — execute compiled schema bytecode against JSON values.
 *
 * Takes a compiled schema (from oas_schema_compile) and validates a yyjson_val
 * or raw JSON string against it, collecting all constraint violations.
 */

#ifndef LIBOAS_OAS_VALIDATOR_H
#define LIBOAS_OAS_VALIDATOR_H

#include <liboas/oas_alloc.h>
#include <liboas/oas_compiler.h>
#include <liboas/oas_error.h>

#include <stdbool.h>
#include <stddef.h>

#include <yyjson.h>

typedef enum {
    OAS_DIR_NONE = 0,    /**< Standalone validation (ignore readOnly/writeOnly) */
    OAS_DIR_REQUEST = 1, /**< Request validation: reject readOnly fields */
    OAS_DIR_RESPONSE = 2 /**< Response validation: reject writeOnly fields */
} oas_validation_direction_t;

typedef struct {
    bool valid;
    oas_error_list_t *errors;
} oas_validation_result_t;

typedef struct {
    const char *name;
    const char *value;
} oas_http_header_t;

typedef struct {
    const char *name;
    const char *value;
} oas_http_query_param_t;

/**
 * @brief Validate a yyjson value against a compiled schema.
 * @param compiled  Compiled schema bytecode.
 * @param value     JSON value to validate.
 * @param result    Receives validation outcome and errors.
 * @param arena     Arena for error allocations (may be nullptr if result->errors is set).
 * @return 0 on success, negative errno on invalid arguments.
 */
[[nodiscard]] int oas_validate(const oas_compiled_schema_t *compiled, yyjson_val *value,
                               oas_validation_result_t *result, oas_arena_t *arena);

/**
 * @brief Parse JSON string and validate against a compiled schema.
 * @param compiled  Compiled schema bytecode.
 * @param json      JSON string to parse and validate.
 * @param len       Length of JSON string in bytes.
 * @param result    Receives validation outcome and errors.
 * @param arena     Arena for error allocations.
 * @return 0 on success, negative errno on invalid arguments or parse failure.
 */
[[nodiscard]] int oas_validate_json(const oas_compiled_schema_t *compiled, const char *json,
                                    size_t len, oas_validation_result_t *result,
                                    oas_arena_t *arena);

/**
 * @brief Validate a yyjson value with direction context.
 * @param compiled  Compiled schema bytecode.
 * @param value     JSON value to validate.
 * @param direction Validation direction (request/response/none).
 * @param result    Receives validation outcome and errors.
 * @param arena     Arena for error allocations.
 * @return 0 on success, negative errno on invalid arguments.
 */
[[nodiscard]] int oas_validate_with_direction(const oas_compiled_schema_t *compiled,
                                              yyjson_val *value,
                                              oas_validation_direction_t direction,
                                              oas_validation_result_t *result, oas_arena_t *arena);

/**
 * @brief Parse JSON string and validate with direction context.
 * @param compiled  Compiled schema bytecode.
 * @param json      JSON string to parse and validate.
 * @param len       Length of JSON string in bytes.
 * @param direction Validation direction (request/response/none).
 * @param result    Receives validation outcome and errors.
 * @param arena     Arena for error allocations.
 * @return 0 on success, negative errno on invalid arguments or parse failure.
 */
[[nodiscard]] int oas_validate_json_with_direction(const oas_compiled_schema_t *compiled,
                                                   const char *json, size_t len,
                                                   oas_validation_direction_t direction,
                                                   oas_validation_result_t *result,
                                                   oas_arena_t *arena);

typedef struct {
    const char *method;       /**< HTTP method (GET, POST, etc.) */
    const char *path;         /**< Request path (e.g., "/pets/123") */
    const char *content_type; /**< Content-Type header (nullable) */
    const char *body;         /**< Request body (nullable) */
    size_t body_len;
    const oas_http_header_t *headers; /**< Request headers (nullable) */
    size_t headers_count;
    const oas_http_query_param_t *query; /**< Query parameters (nullable) */
    size_t query_count;
    const char *query_string; /**< Raw query string (nullable) */
} oas_http_request_t;

typedef struct {
    int status_code;          /**< HTTP status code (200, 404, etc.) */
    const char *content_type; /**< Content-Type header (nullable) */
    const char *body;         /**< Response body (nullable) */
    size_t body_len;
    const oas_http_header_t *headers; /**< Response headers (nullable) */
    size_t headers_count;
} oas_http_response_t;

/**
 * @brief Validate an HTTP request against a compiled OpenAPI document.
 * @param doc     Compiled document.
 * @param req     HTTP request to validate.
 * @param result  Receives validation outcome and errors.
 * @param arena   Arena for error allocations.
 * @return 0 on success, negative errno on invalid arguments.
 */
[[nodiscard]] int oas_validate_request(const oas_compiled_doc_t *doc, const oas_http_request_t *req,
                                       oas_validation_result_t *result, oas_arena_t *arena);

/**
 * @brief Validate an HTTP response against a compiled OpenAPI document.
 * @param doc     Compiled document.
 * @param path    Request path (e.g., "/pets/123").
 * @param method  HTTP method (e.g., "GET").
 * @param resp    HTTP response to validate.
 * @param result  Receives validation outcome and errors.
 * @param arena   Arena for error allocations.
 * @return 0 on success, negative errno on invalid arguments.
 */
[[nodiscard]] int oas_validate_response(const oas_compiled_doc_t *doc, const char *path,
                                        const char *method, const oas_http_response_t *resp,
                                        oas_validation_result_t *result, oas_arena_t *arena);

/**
 * @brief Apply default values from a schema to a JSON object (recursive).
 *
 * For each property in the schema that has a default_value and is missing
 * from the object, inserts the default. Recurses into nested objects.
 *
 * @param schema  Source schema with default values.
 * @param json    JSON string to apply defaults to.
 * @param len     Length of JSON string in bytes.
 * @param out_body  Output: modified body JSON with defaults applied (caller frees with free()).
 * @param out_len   Output: modified body length.
 * @return 0 on success, negative errno on error.
 */
[[nodiscard]] int oas_apply_defaults(const oas_schema_t *schema, const char *json, size_t len,
                                     char **out_body, size_t *out_len);

/**
 * @brief Validate request and apply default values from schema.
 *
 * Creates a mutable copy of the request body JSON, applies default values
 * for missing fields, validates, and returns the modified body.
 *
 * @param doc       Compiled document.
 * @param req       HTTP request (body will NOT be modified).
 * @param result    Validation result.
 * @param arena     Arena for error allocation.
 * @param out_body  Output: modified body JSON with defaults applied (caller frees with free()).
 * @param out_len   Output: modified body length.
 * @return 0 on success, negative errno on error.
 */
[[nodiscard]] int oas_validate_request_with_defaults(const oas_compiled_doc_t *doc,
                                                     const oas_http_request_t *req,
                                                     oas_validation_result_t *result,
                                                     oas_arena_t *arena, char **out_body,
                                                     size_t *out_len);

#endif /* LIBOAS_OAS_VALIDATOR_H */
