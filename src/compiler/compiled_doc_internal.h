/**
 * @file compiled_doc_internal.h
 * @brief Internal compiled document types shared across compiler, validator, and adapter.
 *
 * Single source of truth for struct layout — eliminates duplication across
 * oas_doc_compiler.c, oas_request.c, and oas_adapter.c.
 */
#ifndef LIBOAS_COMPILED_DOC_INTERNAL_H
#define LIBOAS_COMPILED_DOC_INTERNAL_H

#include <liboas/oas_compiler.h>
#include <liboas/oas_regex.h>

#include "core/oas_path_match.h"

typedef struct {
    const char *content_type;
    oas_compiled_schema_t *schema;
} compiled_media_type_t;

typedef struct {
    const char *name;
    const char *in;
    bool required;
    oas_compiled_schema_t *schema;
} compiled_param_t;

typedef struct {
    const char *status_code;
    compiled_media_type_t *content;
    size_t content_count;
} compiled_response_t;

typedef struct {
    const char *path;
    const char *method;
    const char *operation_id;
    compiled_media_type_t *request_body;
    size_t request_body_count;
    bool request_body_required;
    compiled_response_t *responses;
    size_t responses_count;
    compiled_param_t *params;
    size_t params_count;
} compiled_operation_t;

struct oas_compiled_doc {
    oas_path_matcher_t *matcher;
    compiled_operation_t *operations;
    size_t operations_count;
    oas_compiled_schema_t **all_schemas;
    size_t all_schemas_count;
    size_t all_schemas_capacity;
    oas_regex_backend_t *regex;
    bool owns_regex;
    oas_arena_t *arena;
};

#endif /* LIBOAS_COMPILED_DOC_INTERNAL_H */
