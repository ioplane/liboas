#include <liboas/oas_validator.h>

#include "core/oas_path_match.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <yyjson.h>

/* Internal types from oas_doc_compiler.c — must match layout exactly */
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
    oas_arena_t *arena;
};

static const compiled_operation_t *find_operation(const oas_compiled_doc_t *doc,
                                                  const char *template_path, const char *method)
{
    for (size_t i = 0; i < doc->operations_count; i++) {
        if (strcmp(doc->operations[i].path, template_path) == 0 &&
            strcasecmp(doc->operations[i].method, method) == 0) {
            return &doc->operations[i];
        }
    }
    return nullptr;
}

static const compiled_media_type_t *find_content_type(const compiled_media_type_t *entries,
                                                      size_t count, const char *content_type)
{
    if (!content_type || !entries) {
        return nullptr;
    }

    /* Exact match first */
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(entries[i].content_type, content_type) == 0) {
            return &entries[i];
        }
    }

    /* Try matching base type (strip parameters like ;charset=utf-8) */
    size_t ct_len = strlen(content_type);
    const char *semi = memchr(content_type, ';', ct_len);
    if (semi) {
        size_t base_len = (size_t)(semi - content_type);
        for (size_t i = 0; i < count; i++) {
            if (strncasecmp(entries[i].content_type, content_type, base_len) == 0 &&
                entries[i].content_type[base_len] == '\0') {
                return &entries[i];
            }
        }
    }

    return nullptr;
}

static void ensure_errors(oas_validation_result_t *result, oas_arena_t *arena)
{
    if (!result->errors && arena) {
        result->errors = oas_error_list_create(arena);
    }
}

int oas_validate_request(const oas_compiled_doc_t *doc, const oas_http_request_t *req,
                         oas_validation_result_t *result, oas_arena_t *arena)
{
    if (!doc || !req || !result || !req->method || !req->path) {
        return -EINVAL;
    }

    result->valid = true;
    ensure_errors(result, arena);

    /* Match request path against templates */
    if (!doc->matcher) {
        result->valid = false;
        if (result->errors) {
            oas_error_list_add(result->errors, OAS_ERR_SCHEMA, "",
                               "no paths registered in document");
        }
        return 0;
    }

    oas_path_match_result_t match = {0};
    int rc = oas_path_match(doc->matcher, req->path, &match, arena);
    if (rc < 0) {
        return rc;
    }

    if (!match.matched) {
        result->valid = false;
        if (result->errors) {
            oas_error_list_add(result->errors, OAS_ERR_SCHEMA, "", "path not found: %s", req->path);
        }
        return 0;
    }

    /* Find operation by template path + method */
    const compiled_operation_t *op = find_operation(doc, match.template_path, req->method);
    if (!op) {
        result->valid = false;
        if (result->errors) {
            oas_error_list_add(result->errors, OAS_ERR_SCHEMA, "",
                               "method %s not allowed for path %s", req->method,
                               match.template_path);
        }
        return 0;
    }

    /* Validate request body */
    if (op->request_body_required && (!req->body || req->body_len == 0)) {
        result->valid = false;
        if (result->errors) {
            oas_error_list_add(result->errors, OAS_ERR_REQUIRED, "", "request body is required");
        }
        return 0;
    }

    if (req->body && req->body_len > 0 && op->request_body_count > 0) {
        const compiled_media_type_t *mt =
            find_content_type(op->request_body, op->request_body_count, req->content_type);
        if (!mt) {
            result->valid = false;
            if (result->errors) {
                oas_error_list_add(result->errors, OAS_ERR_SCHEMA, "",
                                   "unsupported content type: %s",
                                   req->content_type ? req->content_type : "(none)");
            }
            return 0;
        }

        rc = oas_validate_json(mt->schema, req->body, req->body_len, result, arena);
        if (rc < 0) {
            return rc;
        }
    }

    return 0;
}

static const compiled_response_t *find_response(const compiled_operation_t *op, int status_code)
{
    /* Convert status code to string for comparison */
    char status_str[16];
    (void)snprintf(status_str, sizeof(status_str), "%d", status_code);

    /* Exact status code match */
    for (size_t i = 0; i < op->responses_count; i++) {
        if (strcmp(op->responses[i].status_code, status_str) == 0) {
            return &op->responses[i];
        }
    }

    /* Fallback to "default" */
    for (size_t i = 0; i < op->responses_count; i++) {
        if (strcmp(op->responses[i].status_code, "default") == 0) {
            return &op->responses[i];
        }
    }

    return nullptr;
}

int oas_validate_response(const oas_compiled_doc_t *doc, const char *path, const char *method,
                          const oas_http_response_t *resp, oas_validation_result_t *result,
                          oas_arena_t *arena)
{
    if (!doc || !path || !method || !resp || !result) {
        return -EINVAL;
    }

    result->valid = true;
    ensure_errors(result, arena);

    if (!doc->matcher) {
        result->valid = false;
        if (result->errors) {
            oas_error_list_add(result->errors, OAS_ERR_SCHEMA, "",
                               "no paths registered in document");
        }
        return 0;
    }

    /* Match path */
    oas_path_match_result_t match = {0};
    int rc = oas_path_match(doc->matcher, path, &match, arena);
    if (rc < 0) {
        return rc;
    }

    if (!match.matched) {
        result->valid = false;
        if (result->errors) {
            oas_error_list_add(result->errors, OAS_ERR_SCHEMA, "", "path not found: %s", path);
        }
        return 0;
    }

    /* Find operation */
    const compiled_operation_t *op = find_operation(doc, match.template_path, method);
    if (!op) {
        result->valid = false;
        if (result->errors) {
            oas_error_list_add(result->errors, OAS_ERR_SCHEMA, "",
                               "method %s not allowed for path %s", method, match.template_path);
        }
        return 0;
    }

    /* Find response definition */
    const compiled_response_t *resp_def = find_response(op, resp->status_code);
    if (!resp_def) {
        result->valid = false;
        if (result->errors) {
            oas_error_list_add(result->errors, OAS_ERR_SCHEMA, "",
                               "no response defined for status %d", resp->status_code);
        }
        return 0;
    }

    /* Validate response body */
    if (resp->body && resp->body_len > 0 && resp_def->content_count > 0) {
        const compiled_media_type_t *mt =
            find_content_type(resp_def->content, resp_def->content_count, resp->content_type);
        if (!mt) {
            result->valid = false;
            if (result->errors) {
                oas_error_list_add(result->errors, OAS_ERR_SCHEMA, "",
                                   "unsupported response content type: %s",
                                   resp->content_type ? resp->content_type : "(none)");
            }
            return 0;
        }

        rc = oas_validate_json(mt->schema, resp->body, resp->body_len, result, arena);
        if (rc < 0) {
            return rc;
        }
    }

    return 0;
}
