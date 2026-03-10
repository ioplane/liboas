#include <liboas/oas_validator.h>

#include "compiler/compiled_doc_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <yyjson.h>

constexpr int OAS_DEFAULTS_MAX_DEPTH = 64;

static void apply_defaults_recursive(yyjson_mut_doc *mdoc, yyjson_mut_val *obj,
                                     const oas_schema_t *schema, int depth)
{
    if (!schema || !obj || !yyjson_mut_is_obj(obj) || depth > OAS_DEFAULTS_MAX_DEPTH) {
        return;
    }

    /* Walk the linked list of properties */
    const oas_property_t *prop = schema->properties;
    while (prop) {
        const oas_schema_t *prop_schema = prop->schema;

        /* Follow $ref chains to resolved target */
        if (prop_schema && prop_schema->ref_resolved) {
            prop_schema = prop_schema->ref_resolved;
        }

        yyjson_mut_val *existing = yyjson_mut_obj_get(obj, prop->name);
        if (!existing && prop_schema && prop_schema->default_value) {
            /* Insert default value for missing field */
            yyjson_mut_val *key = yyjson_mut_strcpy(mdoc, prop->name);
            yyjson_mut_val *val = yyjson_val_mut_copy(mdoc, prop_schema->default_value);
            if (key && val) {
                yyjson_mut_obj_add(obj, key, val);
            }
        } else if (existing && yyjson_mut_is_obj(existing) && prop_schema) {
            /* Recurse into nested objects */
            apply_defaults_recursive(mdoc, existing, prop_schema, depth + 1);
        }

        prop = prop->next;
    }
}

int oas_apply_defaults(const oas_schema_t *schema, const char *json, size_t len, char **out_body,
                       size_t *out_len)
{
    if (!schema || !json || !out_body || !out_len) {
        return -EINVAL;
    }

    /* Parse the JSON into an immutable doc first */
    yyjson_doc *idoc = yyjson_read(json, len, 0);
    if (!idoc) {
        return -EINVAL;
    }

    /* Create a mutable copy */
    yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(idoc, nullptr);
    yyjson_doc_free(idoc);
    if (!mdoc) {
        return -ENOMEM;
    }

    yyjson_mut_val *root = yyjson_mut_doc_get_root(mdoc);
    apply_defaults_recursive(mdoc, root, schema, 0);

    /* Serialize back to JSON */
    size_t wlen = 0;
    char *result = yyjson_mut_write(mdoc, 0, &wlen);
    yyjson_mut_doc_free(mdoc);
    if (!result) {
        return -ENOMEM;
    }

    *out_body = result;
    *out_len = wlen;
    return 0;
}

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

int oas_validate_request_with_defaults(const oas_compiled_doc_t *doc, const oas_http_request_t *req,
                                       oas_validation_result_t *result, oas_arena_t *arena,
                                       char **out_body, size_t *out_len)
{
    if (!doc || !req || !result || !req->method || !req->path || !out_body || !out_len) {
        return -EINVAL;
    }

    *out_body = nullptr;
    *out_len = 0;

    /* Run standard validation first */
    int rc = oas_validate_request(doc, req, result, arena);
    if (rc < 0) {
        return rc;
    }

    /* Apply defaults only if body exists */
    if (!req->body || req->body_len == 0) {
        return 0;
    }

    /* Find the matching operation to get the source schema */
    if (!doc->matcher) {
        return 0;
    }

    oas_path_match_result_t match = {0};
    rc = oas_path_match(doc->matcher, req->path, &match, arena);
    if (rc < 0 || !match.matched) {
        return 0;
    }

    const compiled_operation_t *op = find_operation(doc, match.template_path, req->method);
    if (!op || op->request_body_count == 0) {
        return 0;
    }

    const compiled_media_type_t *mt =
        find_content_type(op->request_body, op->request_body_count, req->content_type);
    if (!mt || !mt->source_schema) {
        return 0;
    }

    /* Apply defaults using the source schema */
    return oas_apply_defaults(mt->source_schema, req->body, req->body_len, out_body, out_len);
}
