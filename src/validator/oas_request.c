#include <liboas/oas_validator.h>

#include "core/oas_path_match.h"
#include "core/oas_query.h"

#include <liboas/oas_regex.h>

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

/* Find a header value by name (case-insensitive) */
static const char *find_header(const oas_http_header_t *headers, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(headers[i].name, name) == 0) {
            return headers[i].value;
        }
    }
    return nullptr;
}

/* Find a query param value by name (case-sensitive) */
static const char *find_query_param(const oas_http_query_param_t *params, size_t count,
                                    const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(params[i].name, name) == 0) {
            return params[i].value;
        }
    }
    return nullptr;
}

/* Validate a string parameter value against a compiled schema.
 * Uses a temporary result to avoid clobbering prior failures.
 * Wraps the value as a JSON string if it doesn't look like JSON. */
static int validate_param_value(const oas_compiled_schema_t *schema, const char *value,
                                oas_validation_result_t *result, oas_arena_t *arena)
{
    if (!schema || !value) {
        return 0;
    }

    /* Use temporary result so oas_validate doesn't reset result->valid */
    oas_validation_result_t tmp = {.errors = result->errors};
    int rc;

    /* Try parsing as JSON first (handles numbers, booleans, etc.) */
    size_t vlen = strlen(value);
    yyjson_doc *doc = yyjson_read(value, vlen, 0);
    if (doc) {
        rc = oas_validate(schema, yyjson_doc_get_root(doc), &tmp, arena);
        yyjson_doc_free(doc);
    } else {
        /* Not valid JSON — wrap as JSON string and validate */
        size_t buf_len = vlen + 3; /* quotes + null */
        char *buf = oas_arena_alloc(arena, buf_len, 1);
        if (!buf) {
            return -ENOMEM;
        }
        buf[0] = '"';
        memcpy(buf + 1, value, vlen);
        buf[vlen + 1] = '"';
        buf[vlen + 2] = '\0';

        rc = oas_validate_json(schema, buf, vlen + 2, &tmp, arena);
    }

    if (!tmp.valid) {
        result->valid = false;
    }
    result->errors = tmp.errors;
    return rc;
}

/* Validate header parameters for a request */
static int validate_header_params(const compiled_param_t *params, size_t params_count,
                                  const oas_http_request_t *req, oas_validation_result_t *result,
                                  oas_arena_t *arena)
{
    for (size_t i = 0; i < params_count; i++) {
        if (strcmp(params[i].in, "header") != 0) {
            continue;
        }
        const char *value =
            req->headers ? find_header(req->headers, req->headers_count, params[i].name) : nullptr;
        if (!value) {
            if (params[i].required) {
                result->valid = false;
                if (result->errors) {
                    oas_error_list_add(result->errors, OAS_ERR_REQUIRED, "",
                                       "required header missing: %s", params[i].name);
                }
            }
            continue;
        }
        if (params[i].schema) {
            int rc = validate_param_value(params[i].schema, value, result, arena);
            if (rc < 0) {
                return rc;
            }
        }
    }
    return 0;
}

/* Validate query parameters for a request */
static int validate_query_params(const compiled_param_t *params, size_t params_count,
                                 const oas_http_request_t *req, oas_validation_result_t *result,
                                 oas_arena_t *arena)
{
    /* Build query param list from either parsed params or raw query string */
    const oas_http_query_param_t *qparams = req->query;
    size_t qcount = req->query_count;
    oas_query_pair_t *parsed = nullptr;
    size_t parsed_count = 0;

    if (!qparams && req->query_string) {
        int rc = oas_query_parse(arena, req->query_string, &parsed, &parsed_count);
        if (rc < 0) {
            return rc;
        }
    }

    for (size_t i = 0; i < params_count; i++) {
        if (strcmp(params[i].in, "query") != 0) {
            continue;
        }
        const char *value = nullptr;
        if (qparams) {
            value = find_query_param(qparams, qcount, params[i].name);
        } else if (parsed) {
            for (size_t j = 0; j < parsed_count; j++) {
                if (strcmp(parsed[j].key, params[i].name) == 0) {
                    value = parsed[j].value;
                    break;
                }
            }
        }
        if (!value) {
            if (params[i].required) {
                result->valid = false;
                if (result->errors) {
                    oas_error_list_add(result->errors, OAS_ERR_REQUIRED, "",
                                       "required query parameter missing: %s", params[i].name);
                }
            }
            continue;
        }
        if (params[i].schema) {
            int rc = validate_param_value(params[i].schema, value, result, arena);
            if (rc < 0) {
                return rc;
            }
        }
    }
    return 0;
}

/* Validate path parameters against compiled schemas */
static int validate_path_params(const compiled_param_t *params, size_t params_count,
                                const oas_path_match_result_t *match,
                                oas_validation_result_t *result, oas_arena_t *arena)
{
    for (size_t i = 0; i < params_count; i++) {
        if (strcmp(params[i].in, "path") != 0 || !params[i].schema) {
            continue;
        }
        /* Find the extracted value from path matching */
        const char *value = nullptr;
        for (size_t j = 0; j < match->params_count; j++) {
            if (strcmp(match->params[j].name, params[i].name) == 0) {
                value = match->params[j].value;
                break;
            }
        }
        if (!value) {
            continue;
        }
        int rc = validate_param_value(params[i].schema, value, result, arena);
        if (rc < 0) {
            return rc;
        }
    }
    return 0;
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

    /* Validate parameters (header, query, path) */
    if (op->params_count > 0) {
        rc = validate_header_params(op->params, op->params_count, req, result, arena);
        if (rc < 0) {
            return rc;
        }
        rc = validate_query_params(op->params, op->params_count, req, result, arena);
        if (rc < 0) {
            return rc;
        }
        if (match.params_count > 0) {
            rc = validate_path_params(op->params, op->params_count, &match, result, arena);
            if (rc < 0) {
                return rc;
            }
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
