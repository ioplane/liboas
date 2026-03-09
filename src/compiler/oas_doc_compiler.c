#include "compiled_doc_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

constexpr size_t OAS_SCHEMA_TRACK_INITIAL_CAP = 16;

static int track_schema(oas_compiled_doc_t *doc, oas_compiled_schema_t *cs)
{
    if (doc->all_schemas_count >= doc->all_schemas_capacity) {
        size_t new_cap = doc->all_schemas_capacity == 0 ? OAS_SCHEMA_TRACK_INITIAL_CAP
                                                        : doc->all_schemas_capacity * 2;
        oas_compiled_schema_t **new_arr = realloc(doc->all_schemas, new_cap * sizeof(*new_arr));
        if (!new_arr) {
            return -ENOMEM;
        }
        doc->all_schemas = new_arr;
        doc->all_schemas_capacity = new_cap;
    }
    doc->all_schemas[doc->all_schemas_count++] = cs;
    return 0;
}

static int compile_media_types(oas_compiled_doc_t *cdoc, const oas_media_type_entry_t *entries,
                               size_t count, const oas_compiler_config_t *config,
                               oas_error_list_t *errors, compiled_media_type_t **out,
                               size_t *out_count)
{
    if (count == 0 || !entries) {
        *out = nullptr;
        *out_count = 0;
        return 0;
    }

    compiled_media_type_t *arr =
        oas_arena_alloc(cdoc->arena, count * sizeof(*arr), _Alignof(compiled_media_type_t));
    if (!arr) {
        return -ENOMEM;
    }

    size_t compiled = 0;
    int ret = 0;
    for (size_t i = 0; i < count; i++) {
        if (!entries[i].value || !entries[i].value->schema) {
            continue;
        }

        oas_compiled_schema_t *cs = oas_schema_compile(entries[i].value->schema, config, errors);
        if (!cs) {
            ret = -EINVAL;
            continue; /* accumulate errors from remaining schemas */
        }

        int rc = track_schema(cdoc, cs);
        if (rc < 0) {
            oas_compiled_schema_free(cs);
            return rc;
        }

        arr[compiled].content_type = entries[i].key;
        arr[compiled].schema = cs;
        compiled++;
    }

    *out = arr;
    *out_count = compiled;
    return ret;
}

static int compile_params(oas_compiled_doc_t *cdoc, oas_parameter_t **parameters, size_t count,
                          const oas_compiler_config_t *config, oas_error_list_t *errors,
                          compiled_param_t **out, size_t *out_count)
{
    if (count == 0 || !parameters) {
        *out = nullptr;
        *out_count = 0;
        return 0;
    }

    compiled_param_t *arr =
        oas_arena_alloc(cdoc->arena, count * sizeof(*arr), _Alignof(compiled_param_t));
    if (!arr) {
        return -ENOMEM;
    }

    size_t compiled = 0;
    int ret = 0;
    for (size_t i = 0; i < count; i++) {
        if (!parameters[i] || !parameters[i]->schema) {
            continue;
        }

        oas_compiled_schema_t *cs = oas_schema_compile(parameters[i]->schema, config, errors);
        if (!cs) {
            ret = -EINVAL;
            continue;
        }

        int rc = track_schema(cdoc, cs);
        if (rc < 0) {
            oas_compiled_schema_free(cs);
            return rc;
        }

        arr[compiled].name = parameters[i]->name;
        arr[compiled].in = parameters[i]->in;
        arr[compiled].required = parameters[i]->required;
        arr[compiled].schema = cs;
        compiled++;
    }

    *out = arr;
    *out_count = compiled;
    return ret;
}

static int compile_responses(oas_compiled_doc_t *cdoc, const oas_response_entry_t *entries,
                             size_t count, const oas_compiler_config_t *config,
                             oas_error_list_t *errors, compiled_response_t **out, size_t *out_count)
{
    if (count == 0 || !entries) {
        *out = nullptr;
        *out_count = 0;
        return 0;
    }

    compiled_response_t *arr =
        oas_arena_alloc(cdoc->arena, count * sizeof(*arr), _Alignof(compiled_response_t));
    if (!arr) {
        return -ENOMEM;
    }

    size_t compiled = 0;
    int ret = 0;
    for (size_t i = 0; i < count; i++) {
        if (!entries[i].response) {
            continue;
        }

        arr[compiled].status_code = entries[i].status_code;

        int rc = compile_media_types(cdoc, entries[i].response->content,
                                     entries[i].response->content_count, config, errors,
                                     &arr[compiled].content, &arr[compiled].content_count);
        if (rc == -ENOMEM) {
            return rc;
        }
        if (rc < 0) {
            ret = rc;
        }
        compiled++;
    }

    *out = arr;
    *out_count = compiled;
    return ret;
}

static int compile_operation(oas_compiled_doc_t *cdoc, const char *path, const char *method,
                             const oas_operation_t *op, const oas_compiler_config_t *config,
                             oas_error_list_t *errors, compiled_operation_t *out)
{
    out->path = path;
    out->method = method;
    out->operation_id = op->operation_id;
    out->request_body_required = false;
    int compile_err = 0;

    /* Compile request body */
    if (op->request_body) {
        out->request_body_required = op->request_body->required;
        int rc = compile_media_types(cdoc, op->request_body->content,
                                     op->request_body->content_count, config, errors,
                                     &out->request_body, &out->request_body_count);
        if (rc == -ENOMEM) {
            return rc;
        }
        if (rc < 0) {
            compile_err = rc;
        }
    } else {
        out->request_body = nullptr;
        out->request_body_count = 0;
    }

    /* Compile responses */
    int rc = compile_responses(cdoc, op->responses, op->responses_count, config, errors,
                               &out->responses, &out->responses_count);
    if (rc == -ENOMEM) {
        return rc;
    }
    if (rc < 0) {
        compile_err = rc;
    }

    /* Compile parameters */
    rc = compile_params(cdoc, op->parameters, op->parameters_count, config, errors, &out->params,
                        &out->params_count);
    if (rc == -ENOMEM) {
        return rc;
    }
    if (rc < 0) {
        compile_err = rc;
    }

    return compile_err;
}

/* Count total operations across all paths */
static size_t count_operations(const oas_doc_t *doc)
{
    size_t total = 0;
    for (size_t i = 0; i < doc->paths_count; i++) {
        const oas_path_item_t *item = doc->paths[i].item;
        if (!item) {
            continue;
        }
        if (item->get) {
            total++;
        }
        if (item->post) {
            total++;
        }
        if (item->put) {
            total++;
        }
        if (item->delete_) {
            total++;
        }
        if (item->patch) {
            total++;
        }
        if (item->head) {
            total++;
        }
        if (item->options) {
            total++;
        }
    }
    return total;
}

oas_compiled_doc_t *oas_doc_compile(const oas_doc_t *doc, const oas_compiler_config_t *config,
                                    oas_error_list_t *errors)
{
    if (!doc) {
        return nullptr;
    }

    oas_arena_t *doc_arena = oas_arena_create(0);
    if (!doc_arena) {
        return nullptr;
    }

    size_t total_ops = 0;
    size_t op_idx = 0;

    oas_compiled_doc_t *cdoc =
        oas_arena_alloc(doc_arena, sizeof(*cdoc), _Alignof(oas_compiled_doc_t));
    if (!cdoc) {
        oas_arena_destroy(doc_arena);
        return nullptr;
    }
    memset(cdoc, 0, sizeof(*cdoc));
    cdoc->arena = doc_arena;
    if (config && config->regex) {
        cdoc->regex = config->regex;
        cdoc->owns_regex = !config->borrow_regex;
    }

    /* Build path matcher from templates */
    if (doc->paths_count > 0) {
        const char **templates = oas_arena_alloc(doc_arena, doc->paths_count * sizeof(*templates),
                                                 _Alignof(const char *));
        if (!templates) {
            goto fail;
        }
        for (size_t i = 0; i < doc->paths_count; i++) {
            templates[i] = doc->paths[i].path;
        }
        cdoc->matcher = oas_path_matcher_create(doc_arena, templates, doc->paths_count);
        if (!cdoc->matcher) {
            goto fail;
        }
    }

    /* Allocate operations array */
    total_ops = count_operations(doc);
    if (total_ops > 0) {
        cdoc->operations = oas_arena_alloc(doc_arena, total_ops * sizeof(*cdoc->operations),
                                           _Alignof(compiled_operation_t));
        if (!cdoc->operations) {
            goto fail;
        }
        memset(cdoc->operations, 0, total_ops * sizeof(*cdoc->operations));
    }

    /* Compile each operation */
    int compile_err = 0;
    for (size_t i = 0; i < doc->paths_count; i++) {
        const oas_path_item_t *item = doc->paths[i].item;
        if (!item) {
            continue;
        }
        const char *path = doc->paths[i].path;

        struct {
            const char *method;
            const oas_operation_t *op;
        } methods[] = {
            {"GET", item->get},         {"POST", item->post},   {"PUT", item->put},
            {"DELETE", item->delete_},  {"PATCH", item->patch}, {"HEAD", item->head},
            {"OPTIONS", item->options},
        };

        for (size_t m = 0; m < sizeof(methods) / sizeof(methods[0]); m++) {
            if (!methods[m].op) {
                continue;
            }
            int rc = compile_operation(cdoc, path, methods[m].method, methods[m].op, config, errors,
                                       &cdoc->operations[op_idx]);
            if (rc == -ENOMEM) {
                goto fail;
            }
            if (rc < 0) {
                compile_err = rc;
            }
            op_idx++;
        }
    }
    cdoc->operations_count = op_idx;

    if (compile_err < 0) {
        goto fail;
    }

    return cdoc;

fail:
    oas_compiled_doc_free(cdoc);
    return nullptr;
}

void oas_compiled_doc_free(oas_compiled_doc_t *compiled)
{
    if (!compiled) {
        return;
    }

    /* Free all tracked compiled schemas (heap-allocated) */
    for (size_t i = 0; i < compiled->all_schemas_count; i++) {
        oas_compiled_schema_free(compiled->all_schemas[i]);
    }
    free(compiled->all_schemas);

    /* Free regex backend only if we own it */
    if (compiled->regex && compiled->owns_regex) {
        compiled->regex->destroy(compiled->regex);
    }

    /* Arena owns matcher, operations, and the compiled_doc struct itself */
    oas_arena_t *arena = compiled->arena;
    oas_arena_destroy(arena);
}
