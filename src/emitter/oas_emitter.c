/**
 * @file oas_emitter.c
 * @brief JSON emission for OpenAPI 3.2 documents and schemas.
 */

#include <liboas/oas_emitter.h>

#include "emitter/oas_emitter_internal.h"

#include <liboas/oas_doc.h>
#include <liboas/oas_schema.h>

#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

/* ── Type mask helpers ──────────────────────────────────────────────────── */

static const char *type_name_single(uint8_t bit)
{
    switch ((oas_type_t)bit) {
    case OAS_TYPE_NULL:
        return "null";
    case OAS_TYPE_BOOLEAN:
        return "boolean";
    case OAS_TYPE_INTEGER:
        return "integer";
    case OAS_TYPE_NUMBER:
        return "number";
    case OAS_TYPE_STRING:
        return "string";
    case OAS_TYPE_ARRAY:
        return "array";
    case OAS_TYPE_OBJECT:
        return "object";
    }
    return nullptr;
}

static int popcount8(uint8_t v)
{
    int c = 0;
    while (v) {
        c += (v & 1);
        v >>= 1;
    }
    return c;
}

static yyjson_mut_val *emit_type(yyjson_mut_doc *doc, uint8_t mask)
{
    if (mask == 0) {
        return nullptr;
    }

    if (popcount8(mask) == 1) {
        const char *name = type_name_single(mask);
        return name ? yyjson_mut_str(doc, name) : nullptr;
    }

    /* Multiple types: emit as array */
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    if (!arr) {
        return nullptr;
    }

    for (unsigned bit = 0x01; bit <= 0x40; bit <<= 1) {
        if (mask & bit) {
            const char *name = type_name_single((uint8_t)bit);
            if (name) {
                yyjson_mut_arr_append(arr, yyjson_mut_str(doc, name));
            }
        }
    }
    return arr;
}

/* ── Schema emission ────────────────────────────────────────────────────── */

yyjson_mut_val *oas_emit_build_schema(yyjson_mut_doc *doc, const oas_schema_t *schema)
{
    if (!schema) {
        return nullptr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) {
        return nullptr;
    }

    /* $ref: in OAS 3.1+, siblings are valid alongside $ref */
    if (schema->ref) {
        yyjson_mut_obj_add_str(doc, obj, "$ref", schema->ref);
    }

    /* type */
    if (schema->type_mask) {
        yyjson_mut_val *type_val = emit_type(doc, schema->type_mask);
        if (type_val) {
            yyjson_mut_obj_add_val(doc, obj, "type", type_val);
        }
    }

    /* Metadata */
    if (schema->title) {
        yyjson_mut_obj_add_str(doc, obj, "title", schema->title);
    }
    if (schema->description) {
        yyjson_mut_obj_add_str(doc, obj, "description", schema->description);
    }
    if (schema->format) {
        yyjson_mut_obj_add_str(doc, obj, "format", schema->format);
    }

    /* String constraints */
    if (schema->min_length >= 0) {
        yyjson_mut_obj_add_int(doc, obj, "minLength", schema->min_length);
    }
    if (schema->max_length >= 0) {
        yyjson_mut_obj_add_int(doc, obj, "maxLength", schema->max_length);
    }
    if (schema->pattern) {
        yyjson_mut_obj_add_str(doc, obj, "pattern", schema->pattern);
    }

    /* Numeric constraints */
    if (schema->has_minimum) {
        yyjson_mut_obj_add_real(doc, obj, "minimum", schema->minimum);
    }
    if (schema->has_maximum) {
        yyjson_mut_obj_add_real(doc, obj, "maximum", schema->maximum);
    }
    if (schema->has_exclusive_minimum) {
        yyjson_mut_obj_add_real(doc, obj, "exclusiveMinimum", schema->exclusive_minimum);
    }
    if (schema->has_exclusive_maximum) {
        yyjson_mut_obj_add_real(doc, obj, "exclusiveMaximum", schema->exclusive_maximum);
    }
    if (schema->has_multiple_of) {
        yyjson_mut_obj_add_real(doc, obj, "multipleOf", schema->multiple_of);
    }

    /* Array constraints */
    if (schema->items) {
        yyjson_mut_val *items_val = oas_emit_build_schema(doc, schema->items);
        if (items_val) {
            yyjson_mut_obj_add_val(doc, obj, "items", items_val);
        }
    }
    if (schema->prefix_items && schema->prefix_items_count > 0) {
        yyjson_mut_val *pi_arr = yyjson_mut_arr(doc);
        for (size_t i = 0; i < schema->prefix_items_count; i++) {
            yyjson_mut_val *pi_val = oas_emit_build_schema(doc, schema->prefix_items[i]);
            if (pi_val) {
                yyjson_mut_arr_append(pi_arr, pi_val);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "prefixItems", pi_arr);
    }
    if (schema->min_items >= 0) {
        yyjson_mut_obj_add_int(doc, obj, "minItems", schema->min_items);
    }
    if (schema->max_items >= 0) {
        yyjson_mut_obj_add_int(doc, obj, "maxItems", schema->max_items);
    }
    if (schema->unique_items) {
        yyjson_mut_obj_add_bool(doc, obj, "uniqueItems", true);
    }
    if (schema->contains) {
        yyjson_mut_val *cv = oas_emit_build_schema(doc, schema->contains);
        if (cv) {
            yyjson_mut_obj_add_val(doc, obj, "contains", cv);
        }
    }
    if (schema->min_contains >= 0) {
        yyjson_mut_obj_add_int(doc, obj, "minContains", schema->min_contains);
    }
    if (schema->max_contains >= 0) {
        yyjson_mut_obj_add_int(doc, obj, "maxContains", schema->max_contains);
    }

    /* Object constraints */
    if (schema->properties) {
        yyjson_mut_val *props = yyjson_mut_obj(doc);
        for (const oas_property_t *p = schema->properties; p; p = p->next) {
            yyjson_mut_val *pval = oas_emit_build_schema(doc, p->schema);
            if (pval) {
                yyjson_mut_obj_add_val(doc, props, p->name, pval);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "properties", props);
    }
    if (schema->required && schema->required_count > 0) {
        yyjson_mut_val *req_arr = yyjson_mut_arr(doc);
        for (size_t i = 0; i < schema->required_count; i++) {
            yyjson_mut_arr_append(req_arr, yyjson_mut_str(doc, schema->required[i]));
        }
        yyjson_mut_obj_add_val(doc, obj, "required", req_arr);
    }
    if (schema->additional_properties_bool) {
        /* Boolean form: additionalProperties was set as true/false */
        yyjson_mut_obj_add_bool(doc, obj, "additionalProperties",
                                schema->additional_properties != nullptr);
    } else if (schema->additional_properties) {
        yyjson_mut_val *ap_val = oas_emit_build_schema(doc, schema->additional_properties);
        if (ap_val) {
            yyjson_mut_obj_add_val(doc, obj, "additionalProperties", ap_val);
        }
    }
    if (schema->min_properties >= 0) {
        yyjson_mut_obj_add_int(doc, obj, "minProperties", schema->min_properties);
    }
    if (schema->max_properties >= 0) {
        yyjson_mut_obj_add_int(doc, obj, "maxProperties", schema->max_properties);
    }
    if (schema->property_names) {
        yyjson_mut_val *pn = oas_emit_build_schema(doc, schema->property_names);
        if (pn) {
            yyjson_mut_obj_add_val(doc, obj, "propertyNames", pn);
        }
    }
    if (schema->pattern_properties && schema->pattern_properties_count > 0) {
        yyjson_mut_val *pp = yyjson_mut_obj(doc);
        for (size_t i = 0; i < schema->pattern_properties_count; i++) {
            yyjson_mut_val *pv = oas_emit_build_schema(doc, schema->pattern_properties[i].schema);
            if (pv) {
                yyjson_mut_obj_add_val(doc, pp, schema->pattern_properties[i].pattern, pv);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "patternProperties", pp);
    }
    if (schema->dependent_required && schema->dependent_required_count > 0) {
        yyjson_mut_val *dr = yyjson_mut_obj(doc);
        for (size_t i = 0; i < schema->dependent_required_count; i++) {
            yyjson_mut_val *arr = yyjson_mut_arr(doc);
            for (size_t j = 0; j < schema->dependent_required[i].required_count; j++) {
                yyjson_mut_arr_append(
                    arr, yyjson_mut_str(doc, schema->dependent_required[i].required[j]));
            }
            yyjson_mut_obj_add_val(doc, dr, schema->dependent_required[i].property, arr);
        }
        yyjson_mut_obj_add_val(doc, obj, "dependentRequired", dr);
    }
    if (schema->dependent_schemas && schema->dependent_schemas_count > 0) {
        yyjson_mut_val *ds = yyjson_mut_obj(doc);
        for (size_t i = 0; i < schema->dependent_schemas_count; i++) {
            yyjson_mut_val *sv = oas_emit_build_schema(doc, schema->dependent_schemas[i].schema);
            if (sv) {
                yyjson_mut_obj_add_val(doc, ds, schema->dependent_schemas[i].property, sv);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "dependentSchemas", ds);
    }

    /* Composition */
    if (schema->all_of && schema->all_of_count > 0) {
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        for (size_t i = 0; i < schema->all_of_count; i++) {
            yyjson_mut_val *v = oas_emit_build_schema(doc, schema->all_of[i]);
            if (v) {
                yyjson_mut_arr_append(arr, v);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "allOf", arr);
    }
    if (schema->any_of && schema->any_of_count > 0) {
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        for (size_t i = 0; i < schema->any_of_count; i++) {
            yyjson_mut_val *v = oas_emit_build_schema(doc, schema->any_of[i]);
            if (v) {
                yyjson_mut_arr_append(arr, v);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "anyOf", arr);
    }
    if (schema->one_of && schema->one_of_count > 0) {
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        for (size_t i = 0; i < schema->one_of_count; i++) {
            yyjson_mut_val *v = oas_emit_build_schema(doc, schema->one_of[i]);
            if (v) {
                yyjson_mut_arr_append(arr, v);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "oneOf", arr);
    }
    if (schema->not_schema) {
        yyjson_mut_val *nv = oas_emit_build_schema(doc, schema->not_schema);
        if (nv) {
            yyjson_mut_obj_add_val(doc, obj, "not", nv);
        }
    }

    /* Conditional */
    if (schema->if_schema) {
        yyjson_mut_val *v = oas_emit_build_schema(doc, schema->if_schema);
        if (v) {
            yyjson_mut_obj_add_val(doc, obj, "if", v);
        }
    }
    if (schema->then_schema) {
        yyjson_mut_val *v = oas_emit_build_schema(doc, schema->then_schema);
        if (v) {
            yyjson_mut_obj_add_val(doc, obj, "then", v);
        }
    }
    if (schema->else_schema) {
        yyjson_mut_val *v = oas_emit_build_schema(doc, schema->else_schema);
        if (v) {
            yyjson_mut_obj_add_val(doc, obj, "else", v);
        }
    }

    /* Enum/const (copy from immutable yyjson_val) */
    if (schema->enum_values) {
        yyjson_mut_val *ev = yyjson_val_mut_copy(doc, schema->enum_values);
        if (ev) {
            yyjson_mut_obj_add_val(doc, obj, "enum", ev);
        }
    }
    if (schema->const_value) {
        yyjson_mut_val *cv = yyjson_val_mut_copy(doc, schema->const_value);
        if (cv) {
            yyjson_mut_obj_add_val(doc, obj, "const", cv);
        }
    }

    /* Default */
    if (schema->default_value) {
        yyjson_mut_val *dv = yyjson_val_mut_copy(doc, schema->default_value);
        if (dv) {
            yyjson_mut_obj_add_val(doc, obj, "default", dv);
        }
    }

    /* Nullable */
    if (schema->nullable) {
        yyjson_mut_obj_add_bool(doc, obj, "nullable", true);
    }

    /* Read/Write only */
    if (schema->read_only) {
        yyjson_mut_obj_add_bool(doc, obj, "readOnly", true);
    }
    if (schema->write_only) {
        yyjson_mut_obj_add_bool(doc, obj, "writeOnly", true);
    }
    if (schema->deprecated) {
        yyjson_mut_obj_add_bool(doc, obj, "deprecated", true);
    }

    /* Discriminator */
    if (schema->discriminator_property) {
        yyjson_mut_val *disc = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, disc, "propertyName", schema->discriminator_property);
        if (schema->discriminator_mapping && schema->discriminator_mapping_count > 0) {
            yyjson_mut_val *mapping = yyjson_mut_obj(doc);
            for (size_t i = 0; i < schema->discriminator_mapping_count; i++) {
                yyjson_mut_obj_add_str(doc, mapping, schema->discriminator_mapping[i].key,
                                       schema->discriminator_mapping[i].ref);
            }
            yyjson_mut_obj_add_val(doc, disc, "mapping", mapping);
        }
        yyjson_mut_obj_add_val(doc, obj, "discriminator", disc);
    }

    return obj;
}

/* ── Parameter emission ─────────────────────────────────────────────────── */

static yyjson_mut_val *emit_parameter(yyjson_mut_doc *doc, const oas_parameter_t *param)
{
    if (!param) {
        return nullptr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) {
        return nullptr;
    }

    if (param->name) {
        yyjson_mut_obj_add_str(doc, obj, "name", param->name);
    }
    if (param->in) {
        yyjson_mut_obj_add_str(doc, obj, "in", param->in);
    }
    if (param->description) {
        yyjson_mut_obj_add_str(doc, obj, "description", param->description);
    }
    if (param->required) {
        yyjson_mut_obj_add_bool(doc, obj, "required", true);
    }
    if (param->deprecated) {
        yyjson_mut_obj_add_bool(doc, obj, "deprecated", true);
    }
    if (param->schema) {
        yyjson_mut_val *sv = oas_emit_build_schema(doc, param->schema);
        if (sv) {
            yyjson_mut_obj_add_val(doc, obj, "schema", sv);
        }
    }

    return obj;
}

/* ── Media type emission ────────────────────────────────────────────────── */

static yyjson_mut_val *emit_media_type_content(yyjson_mut_doc *doc,
                                               const oas_media_type_entry_t *entries, size_t count)
{
    if (!entries || count == 0) {
        return nullptr;
    }

    yyjson_mut_val *content = yyjson_mut_obj(doc);
    if (!content) {
        return nullptr;
    }

    for (size_t i = 0; i < count; i++) {
        yyjson_mut_val *mt_obj = yyjson_mut_obj(doc);
        if (entries[i].value && entries[i].value->schema) {
            yyjson_mut_val *sv = oas_emit_build_schema(doc, entries[i].value->schema);
            if (sv) {
                yyjson_mut_obj_add_val(doc, mt_obj, "schema", sv);
            }
        }
        yyjson_mut_obj_add_val(doc, content, entries[i].key, mt_obj);
    }

    return content;
}

/* ── Request body emission ──────────────────────────────────────────────── */

static yyjson_mut_val *emit_request_body(yyjson_mut_doc *doc, const oas_request_body_t *rb)
{
    if (!rb) {
        return nullptr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) {
        return nullptr;
    }

    if (rb->description) {
        yyjson_mut_obj_add_str(doc, obj, "description", rb->description);
    }
    if (rb->required) {
        yyjson_mut_obj_add_bool(doc, obj, "required", true);
    }
    if (rb->content && rb->content_count > 0) {
        yyjson_mut_val *content = emit_media_type_content(doc, rb->content, rb->content_count);
        if (content) {
            yyjson_mut_obj_add_val(doc, obj, "content", content);
        }
    }

    return obj;
}

/* ── Response emission ──────────────────────────────────────────────────── */

static yyjson_mut_val *emit_responses(yyjson_mut_doc *doc, const oas_response_entry_t *entries,
                                      size_t count)
{
    if (!entries || count == 0) {
        return nullptr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) {
        return nullptr;
    }

    for (size_t i = 0; i < count; i++) {
        yyjson_mut_val *resp = yyjson_mut_obj(doc);
        if (entries[i].response) {
            if (entries[i].response->description) {
                yyjson_mut_obj_add_str(doc, resp, "description", entries[i].response->description);
            }
            if (entries[i].response->content && entries[i].response->content_count > 0) {
                yyjson_mut_val *content = emit_media_type_content(
                    doc, entries[i].response->content, entries[i].response->content_count);
                if (content) {
                    yyjson_mut_obj_add_val(doc, resp, "content", content);
                }
            }
        }
        yyjson_mut_obj_add_val(doc, obj, entries[i].status_code, resp);
    }

    return obj;
}

/* ── Operation emission ─────────────────────────────────────────────────── */

static yyjson_mut_val *emit_operation(yyjson_mut_doc *doc, const oas_operation_t *op)
{
    if (!op) {
        return nullptr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) {
        return nullptr;
    }

    if (op->operation_id) {
        yyjson_mut_obj_add_str(doc, obj, "operationId", op->operation_id);
    }
    if (op->summary) {
        yyjson_mut_obj_add_str(doc, obj, "summary", op->summary);
    }
    if (op->description) {
        yyjson_mut_obj_add_str(doc, obj, "description", op->description);
    }

    if (op->tags && op->tags_count > 0) {
        yyjson_mut_val *tags = yyjson_mut_arr(doc);
        for (size_t i = 0; i < op->tags_count; i++) {
            yyjson_mut_arr_append(tags, yyjson_mut_str(doc, op->tags[i]));
        }
        yyjson_mut_obj_add_val(doc, obj, "tags", tags);
    }

    if (op->parameters && op->parameters_count > 0) {
        yyjson_mut_val *params = yyjson_mut_arr(doc);
        for (size_t i = 0; i < op->parameters_count; i++) {
            yyjson_mut_val *pv = emit_parameter(doc, op->parameters[i]);
            if (pv) {
                yyjson_mut_arr_append(params, pv);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "parameters", params);
    }

    if (op->request_body) {
        yyjson_mut_val *rb = emit_request_body(doc, op->request_body);
        if (rb) {
            yyjson_mut_obj_add_val(doc, obj, "requestBody", rb);
        }
    }

    if (op->responses && op->responses_count > 0) {
        yyjson_mut_val *resp = emit_responses(doc, op->responses, op->responses_count);
        if (resp) {
            yyjson_mut_obj_add_val(doc, obj, "responses", resp);
        }
    }

    if (op->security && op->security_count > 0) {
        yyjson_mut_val *sec_arr = yyjson_mut_arr(doc);
        for (size_t i = 0; i < op->security_count; i++) {
            yyjson_mut_val *sec_obj = yyjson_mut_obj(doc);
            if (op->security[i]) {
                yyjson_mut_val *scopes = yyjson_mut_arr(doc);
                for (size_t j = 0; j < op->security[i]->scopes_count; j++) {
                    yyjson_mut_arr_append(scopes, yyjson_mut_str(doc, op->security[i]->scopes[j]));
                }
                yyjson_mut_obj_add_val(doc, sec_obj, op->security[i]->name, scopes);
            }
            yyjson_mut_arr_append(sec_arr, sec_obj);
        }
        yyjson_mut_obj_add_val(doc, obj, "security", sec_arr);
    }

    if (op->deprecated) {
        yyjson_mut_obj_add_bool(doc, obj, "deprecated", true);
    }

    return obj;
}

/* ── Path item emission ─────────────────────────────────────────────────── */

static yyjson_mut_val *emit_path_item(yyjson_mut_doc *doc, const oas_path_item_t *item)
{
    if (!item) {
        return nullptr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) {
        return nullptr;
    }

    struct {
        const char *method;
        const oas_operation_t *op;
    } methods[] = {
        {"get", item->get},         {"post", item->post},   {"put", item->put},
        {"delete", item->delete_},  {"patch", item->patch}, {"head", item->head},
        {"options", item->options},
    };

    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
        if (methods[i].op) {
            yyjson_mut_val *ov = emit_operation(doc, methods[i].op);
            if (ov) {
                yyjson_mut_obj_add_val(doc, obj, methods[i].method, ov);
            }
        }
    }

    if (item->parameters && item->parameters_count > 0) {
        yyjson_mut_val *params = yyjson_mut_arr(doc);
        for (size_t i = 0; i < item->parameters_count; i++) {
            yyjson_mut_val *pv = emit_parameter(doc, item->parameters[i]);
            if (pv) {
                yyjson_mut_arr_append(params, pv);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "parameters", params);
    }

    return obj;
}

/* ── Info emission ──────────────────────────────────────────────────────── */

static yyjson_mut_val *emit_info(yyjson_mut_doc *doc, const oas_info_t *info)
{
    if (!info) {
        return nullptr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) {
        return nullptr;
    }

    if (info->title) {
        yyjson_mut_obj_add_str(doc, obj, "title", info->title);
    }
    if (info->summary) {
        yyjson_mut_obj_add_str(doc, obj, "summary", info->summary);
    }
    if (info->description) {
        yyjson_mut_obj_add_str(doc, obj, "description", info->description);
    }
    if (info->version) {
        yyjson_mut_obj_add_str(doc, obj, "version", info->version);
    }
    if (info->terms_of_service) {
        yyjson_mut_obj_add_str(doc, obj, "termsOfService", info->terms_of_service);
    }

    if (info->contact) {
        yyjson_mut_val *c = yyjson_mut_obj(doc);
        if (info->contact->name) {
            yyjson_mut_obj_add_str(doc, c, "name", info->contact->name);
        }
        if (info->contact->url) {
            yyjson_mut_obj_add_str(doc, c, "url", info->contact->url);
        }
        if (info->contact->email) {
            yyjson_mut_obj_add_str(doc, c, "email", info->contact->email);
        }
        yyjson_mut_obj_add_val(doc, obj, "contact", c);
    }

    if (info->license) {
        yyjson_mut_val *l = yyjson_mut_obj(doc);
        if (info->license->name) {
            yyjson_mut_obj_add_str(doc, l, "name", info->license->name);
        }
        if (info->license->url) {
            yyjson_mut_obj_add_str(doc, l, "url", info->license->url);
        }
        if (info->license->identifier) {
            yyjson_mut_obj_add_str(doc, l, "identifier", info->license->identifier);
        }
        yyjson_mut_obj_add_val(doc, obj, "license", l);
    }

    return obj;
}

/* ── Server emission ────────────────────────────────────────────────────── */

static yyjson_mut_val *emit_server(yyjson_mut_doc *doc, const oas_server_t *server)
{
    if (!server) {
        return nullptr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) {
        return nullptr;
    }

    if (server->url) {
        yyjson_mut_obj_add_str(doc, obj, "url", server->url);
    }
    if (server->description) {
        yyjson_mut_obj_add_str(doc, obj, "description", server->description);
    }

    if (server->variables && server->variables_count > 0) {
        yyjson_mut_val *vars = yyjson_mut_obj(doc);
        for (size_t i = 0; i < server->variables_count; i++) {
            const oas_server_var_t *sv = server->variables[i];
            if (!sv || !sv->name) {
                continue;
            }
            yyjson_mut_val *var_obj = yyjson_mut_obj(doc);
            if (sv->default_value) {
                yyjson_mut_obj_add_str(doc, var_obj, "default", sv->default_value);
            }
            if (sv->description) {
                yyjson_mut_obj_add_str(doc, var_obj, "description", sv->description);
            }
            if (sv->enum_values && sv->enum_count > 0) {
                yyjson_mut_val *ev = yyjson_mut_arr(doc);
                for (size_t j = 0; j < sv->enum_count; j++) {
                    yyjson_mut_arr_append(ev, yyjson_mut_str(doc, sv->enum_values[j]));
                }
                yyjson_mut_obj_add_val(doc, var_obj, "enum", ev);
            }
            yyjson_mut_obj_add_val(doc, vars, sv->name, var_obj);
        }
        yyjson_mut_obj_add_val(doc, obj, "variables", vars);
    }

    return obj;
}

/* ── Components emission ────────────────────────────────────────────────── */

static yyjson_mut_val *emit_components(yyjson_mut_doc *doc, const oas_components_t *comp)
{
    if (!comp) {
        return nullptr;
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) {
        return nullptr;
    }

    if (comp->schemas && comp->schemas_count > 0) {
        yyjson_mut_val *schemas = yyjson_mut_obj(doc);
        for (size_t i = 0; i < comp->schemas_count; i++) {
            yyjson_mut_val *sv = oas_emit_build_schema(doc, comp->schemas[i].schema);
            if (sv) {
                yyjson_mut_obj_add_val(doc, schemas, comp->schemas[i].name, sv);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "schemas", schemas);
    }

    if (comp->security_schemes && comp->security_schemes_count > 0) {
        yyjson_mut_val *schemes = yyjson_mut_obj(doc);
        for (size_t i = 0; i < comp->security_schemes_count; i++) {
            const oas_security_scheme_entry_t *e = &comp->security_schemes[i];
            if (!e->scheme) {
                continue;
            }
            yyjson_mut_val *sv = yyjson_mut_obj(doc);
            if (e->scheme->type) {
                yyjson_mut_obj_add_str(doc, sv, "type", e->scheme->type);
            }
            if (e->scheme->name) {
                yyjson_mut_obj_add_str(doc, sv, "name", e->scheme->name);
            }
            if (e->scheme->in) {
                yyjson_mut_obj_add_str(doc, sv, "in", e->scheme->in);
            }
            if (e->scheme->scheme) {
                yyjson_mut_obj_add_str(doc, sv, "scheme", e->scheme->scheme);
            }
            if (e->scheme->bearer_format) {
                yyjson_mut_obj_add_str(doc, sv, "bearerFormat", e->scheme->bearer_format);
            }
            if (e->scheme->description) {
                yyjson_mut_obj_add_str(doc, sv, "description", e->scheme->description);
            }
            yyjson_mut_obj_add_val(doc, schemes, e->name, sv);
        }
        yyjson_mut_obj_add_val(doc, obj, "securitySchemes", schemes);
    }

    /* responses */
    if (comp->responses && comp->responses_count > 0) {
        yyjson_mut_val *responses = yyjson_mut_obj(doc);
        for (size_t i = 0; i < comp->responses_count; i++) {
            yyjson_mut_val *rv = yyjson_mut_obj(doc);
            if (comp->responses[i].response) {
                if (comp->responses[i].response->description) {
                    yyjson_mut_obj_add_str(doc, rv, "description",
                                           comp->responses[i].response->description);
                }
                if (comp->responses[i].response->content &&
                    comp->responses[i].response->content_count > 0) {
                    yyjson_mut_val *content =
                        emit_media_type_content(doc, comp->responses[i].response->content,
                                                comp->responses[i].response->content_count);
                    if (content) {
                        yyjson_mut_obj_add_val(doc, rv, "content", content);
                    }
                }
            }
            yyjson_mut_obj_add_val(doc, responses, comp->responses[i].name, rv);
        }
        yyjson_mut_obj_add_val(doc, obj, "responses", responses);
    }

    /* parameters */
    if (comp->parameters && comp->parameters_count > 0) {
        yyjson_mut_val *params = yyjson_mut_obj(doc);
        for (size_t i = 0; i < comp->parameters_count; i++) {
            yyjson_mut_val *pv = emit_parameter(doc, comp->parameters[i].parameter);
            if (pv) {
                yyjson_mut_obj_add_val(doc, params, comp->parameters[i].name, pv);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "parameters", params);
    }

    /* requestBodies */
    if (comp->request_bodies && comp->request_bodies_count > 0) {
        yyjson_mut_val *bodies = yyjson_mut_obj(doc);
        for (size_t i = 0; i < comp->request_bodies_count; i++) {
            yyjson_mut_val *rv = emit_request_body(doc, comp->request_bodies[i].request_body);
            if (rv) {
                yyjson_mut_obj_add_val(doc, bodies, comp->request_bodies[i].name, rv);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "requestBodies", bodies);
    }

    /* headers */
    if (comp->headers && comp->headers_count > 0) {
        yyjson_mut_val *headers = yyjson_mut_obj(doc);
        for (size_t i = 0; i < comp->headers_count; i++) {
            yyjson_mut_val *hv = emit_parameter(doc, comp->headers[i].header);
            if (hv) {
                yyjson_mut_obj_add_val(doc, headers, comp->headers[i].name, hv);
            }
        }
        yyjson_mut_obj_add_val(doc, obj, "headers", headers);
    }

    return obj;
}

/* ── Document emission ──────────────────────────────────────────────────── */

yyjson_mut_val *oas_emit_build_doc(yyjson_mut_doc *doc, const oas_doc_t *oas)
{
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (!root) {
        return nullptr;
    }

    /* openapi version */
    if (oas->openapi) {
        yyjson_mut_obj_add_str(doc, root, "openapi", oas->openapi);
    }

    /* info */
    yyjson_mut_val *info = emit_info(doc, oas->info);
    if (info) {
        yyjson_mut_obj_add_val(doc, root, "info", info);
    }

    /* servers */
    if (oas->servers && oas->servers_count > 0) {
        yyjson_mut_val *servers = yyjson_mut_arr(doc);
        for (size_t i = 0; i < oas->servers_count; i++) {
            yyjson_mut_val *sv = emit_server(doc, oas->servers[i]);
            if (sv) {
                yyjson_mut_arr_append(servers, sv);
            }
        }
        yyjson_mut_obj_add_val(doc, root, "servers", servers);
    }

    /* paths */
    if (oas->paths && oas->paths_count > 0) {
        yyjson_mut_val *paths = yyjson_mut_obj(doc);
        for (size_t i = 0; i < oas->paths_count; i++) {
            yyjson_mut_val *pi = emit_path_item(doc, oas->paths[i].item);
            if (pi) {
                yyjson_mut_obj_add_val(doc, paths, oas->paths[i].path, pi);
            }
        }
        yyjson_mut_obj_add_val(doc, root, "paths", paths);
    }

    /* components */
    if (oas->components) {
        yyjson_mut_val *comp = emit_components(doc, oas->components);
        if (comp) {
            yyjson_mut_obj_add_val(doc, root, "components", comp);
        }
    }

    /* tags */
    if (oas->tags && oas->tags_count > 0) {
        yyjson_mut_val *tags = yyjson_mut_arr(doc);
        for (size_t i = 0; i < oas->tags_count; i++) {
            if (!oas->tags[i]) {
                continue;
            }
            yyjson_mut_val *tobj = yyjson_mut_obj(doc);
            if (oas->tags[i]->name) {
                yyjson_mut_obj_add_str(doc, tobj, "name", oas->tags[i]->name);
            }
            if (oas->tags[i]->description) {
                yyjson_mut_obj_add_str(doc, tobj, "description", oas->tags[i]->description);
            }
            yyjson_mut_arr_append(tags, tobj);
        }
        yyjson_mut_obj_add_val(doc, root, "tags", tags);
    }

    /* security */
    if (oas->security && oas->security_count > 0) {
        yyjson_mut_val *sec = yyjson_mut_arr(doc);
        for (size_t i = 0; i < oas->security_count; i++) {
            yyjson_mut_val *sobj = yyjson_mut_obj(doc);
            if (oas->security[i]) {
                yyjson_mut_val *scopes = yyjson_mut_arr(doc);
                for (size_t j = 0; j < oas->security[i]->scopes_count; j++) {
                    yyjson_mut_arr_append(scopes, yyjson_mut_str(doc, oas->security[i]->scopes[j]));
                }
                yyjson_mut_obj_add_val(doc, sobj, oas->security[i]->name, scopes);
            }
            yyjson_mut_arr_append(sec, sobj);
        }
        yyjson_mut_obj_add_val(doc, root, "security", sec);
    }

    return root;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

char *oas_doc_emit_json(const oas_doc_t *oas, const oas_emit_options_t *options, size_t *out_len)
{
    if (!oas) {
        return nullptr;
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    if (!doc) {
        return nullptr;
    }

    yyjson_mut_val *root = oas_emit_build_doc(doc, oas);
    if (!root) {
        yyjson_mut_doc_free(doc);
        return nullptr;
    }

    yyjson_mut_doc_set_root(doc, root);

    yyjson_write_flag flags = 0;
    if (options && options->pretty) {
        flags |= YYJSON_WRITE_PRETTY;
    }

    size_t len = 0;
    char *json = yyjson_mut_write(doc, flags, &len);
    if (out_len) {
        *out_len = len;
    }

    yyjson_mut_doc_free(doc);
    return json;
}

char *oas_schema_emit_json(const oas_schema_t *schema, const oas_emit_options_t *options,
                           size_t *out_len)
{
    if (!schema) {
        return nullptr;
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    if (!doc) {
        return nullptr;
    }

    yyjson_mut_val *root = oas_emit_build_schema(doc, schema);
    if (!root) {
        yyjson_mut_doc_free(doc);
        return nullptr;
    }

    yyjson_mut_doc_set_root(doc, root);

    yyjson_write_flag flags = 0;
    if (options && options->pretty) {
        flags |= YYJSON_WRITE_PRETTY;
    }

    size_t len = 0;
    char *json = yyjson_mut_write(doc, flags, &len);
    if (out_len) {
        *out_len = len;
    }

    yyjson_mut_doc_free(doc);
    return json;
}

void oas_emit_free(char *json)
{
    free(json);
}
