#include "oas_ref.h"

#include "core/oas_jsonptr.h"
#include "parser/oas_schema_parser.h"

#include <errno.h>
#include <string.h>

/* Max nesting depth for ref chains (A -> B -> C ...) */
#define OAS_MAX_REF_DEPTH 64

/* Simple visited set for cycle detection (arena-backed array) */
typedef struct {
    const char **refs;
    size_t count;
    size_t capacity;
} ref_visited_t;

struct oas_ref_ctx {
    oas_arena_t *arena;
    yyjson_val *root;
    ref_visited_t visited;
};

oas_ref_ctx_t *oas_ref_ctx_create(oas_arena_t *arena, yyjson_val *root)
{
    if (!arena || !root) {
        return nullptr;
    }
    oas_ref_ctx_t *ctx = oas_arena_alloc(arena, sizeof(*ctx), _Alignof(oas_ref_ctx_t));
    if (!ctx) {
        return nullptr;
    }
    ctx->arena = arena;
    ctx->root = root;
    ctx->visited.count = 0;
    ctx->visited.capacity = 16;
    ctx->visited.refs = oas_arena_alloc(arena, sizeof(const char *) * 16, _Alignof(const char *));
    if (!ctx->visited.refs) {
        return nullptr;
    }
    return ctx;
}

static bool visited_contains(const ref_visited_t *v, const char *ref)
{
    for (size_t i = 0; i < v->count; i++) {
        if (strcmp(v->refs[i], ref) == 0) {
            return true;
        }
    }
    return false;
}

static int visited_push(ref_visited_t *v, const char *ref, oas_arena_t *arena)
{
    if (v->count >= v->capacity) {
        size_t new_cap = v->capacity * 2;
        const char **new_refs =
            oas_arena_alloc(arena, sizeof(const char *) * new_cap, _Alignof(const char *));
        if (!new_refs) {
            return -ENOMEM;
        }
        memcpy(new_refs, v->refs, sizeof(const char *) * v->count);
        v->refs = new_refs;
        v->capacity = new_cap;
    }
    v->refs[v->count++] = ref;
    return 0;
}

static void visited_pop(ref_visited_t *v)
{
    if (v->count > 0) {
        v->count--;
    }
}

int oas_ref_resolve(oas_ref_ctx_t *ctx, const char *ref, yyjson_val **out, oas_error_list_t *errors)
{
    if (!ctx || !ref || !out) {
        return -EINVAL;
    }

    /* Only local fragment refs supported (#/...) */
    if (ref[0] != '#') {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, ref, "external $ref not supported: %s", ref);
        }
        return -ENOTSUP;
    }

    /* Cycle detection */
    if (visited_contains(&ctx->visited, ref)) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, ref, "circular $ref detected: %s", ref);
        }
        return -ELOOP;
    }

    /* Depth limit */
    if (ctx->visited.count >= OAS_MAX_REF_DEPTH) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, ref, "$ref chain too deep (max %d): %s",
                               OAS_MAX_REF_DEPTH, ref);
        }
        return -ELOOP;
    }

    /* Extract JSON Pointer from fragment */
    const char *pointer = oas_jsonptr_from_ref(ref);
    yyjson_val *val = oas_jsonptr_resolve_ex(ctx->root, pointer, errors);
    if (!val) {
        return -ENOENT;
    }

    /* If the resolved value itself has a $ref, follow the chain */
    if (yyjson_is_obj(val)) {
        yyjson_val *nested_ref = yyjson_obj_get(val, "$ref");
        if (nested_ref && yyjson_is_str(nested_ref)) {
            int rc = visited_push(&ctx->visited, ref, ctx->arena);
            if (rc < 0) {
                return rc;
            }
            rc = oas_ref_resolve(ctx, yyjson_get_str(nested_ref), out, errors);
            visited_pop(&ctx->visited);
            return rc;
        }
    }

    *out = val;
    return 0;
}

/* Find a component schema by name */
static oas_schema_t *find_component_schema(oas_doc_t *doc, const char *name)
{
    if (!doc->components) {
        return nullptr;
    }
    for (size_t i = 0; i < doc->components->schemas_count; i++) {
        if (doc->components->schemas[i].name &&
            strcmp(doc->components->schemas[i].name, name) == 0) {
            return doc->components->schemas[i].schema;
        }
    }
    return nullptr;
}

/* Recursively resolve $ref in a schema tree */
static int resolve_schema_refs(oas_ref_ctx_t *ctx, oas_doc_t *doc, oas_schema_t *schema,
                               oas_error_list_t *errors)
{
    if (!schema) {
        return 0;
    }

    /* Resolve this schema's own $ref */
    if (schema->ref) {
        /* Try to find as component schema first */
        const char *pointer = oas_jsonptr_from_ref(schema->ref);

        /* Check if it's #/components/schemas/<Name> */
        const char *prefix = "/components/schemas/";
        size_t prefix_len = strlen(prefix);
        if (strncmp(pointer, prefix, prefix_len) == 0) {
            const char *name = pointer + prefix_len;
            oas_schema_t *target = find_component_schema(doc, name);
            if (target) {
                schema->ref_resolved = target;
                return 0;
            }
        }

        /* Fallback: resolve via JSON Pointer and parse */
        yyjson_val *val = nullptr;
        int rc = oas_ref_resolve(ctx, schema->ref, &val, errors);
        if (rc < 0) {
            return rc;
        }

        /* Parse the resolved JSON into a schema */
        if (yyjson_is_obj(val)) {
            oas_schema_t *resolved = oas_schema_parse(ctx->arena, val, errors);
            if (resolved) {
                schema->ref_resolved = resolved;
            }
        }
    }

    /* Recurse into sub-schemas */
    if (schema->items) {
        int rc = resolve_schema_refs(ctx, doc, schema->items, errors);
        if (rc < 0) {
            return rc;
        }
    }

    for (size_t i = 0; i < schema->prefix_items_count; i++) {
        if (schema->prefix_items[i]) {
            int rc = resolve_schema_refs(ctx, doc, schema->prefix_items[i], errors);
            if (rc < 0) {
                return rc;
            }
        }
    }

    /* Properties (linked list) */
    for (oas_property_t *prop = schema->properties; prop; prop = prop->next) {
        int rc = resolve_schema_refs(ctx, doc, prop->schema, errors);
        if (rc < 0) {
            return rc;
        }
    }

    if (schema->additional_properties) {
        int rc = resolve_schema_refs(ctx, doc, schema->additional_properties, errors);
        if (rc < 0) {
            return rc;
        }
    }

    /* Composition */
    for (size_t i = 0; i < schema->all_of_count; i++) {
        int rc = resolve_schema_refs(ctx, doc, schema->all_of[i], errors);
        if (rc < 0) {
            return rc;
        }
    }
    for (size_t i = 0; i < schema->any_of_count; i++) {
        int rc = resolve_schema_refs(ctx, doc, schema->any_of[i], errors);
        if (rc < 0) {
            return rc;
        }
    }
    for (size_t i = 0; i < schema->one_of_count; i++) {
        int rc = resolve_schema_refs(ctx, doc, schema->one_of[i], errors);
        if (rc < 0) {
            return rc;
        }
    }

    /* Conditional */
    if (schema->not_schema) {
        int rc = resolve_schema_refs(ctx, doc, schema->not_schema, errors);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->if_schema) {
        int rc = resolve_schema_refs(ctx, doc, schema->if_schema, errors);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->then_schema) {
        int rc = resolve_schema_refs(ctx, doc, schema->then_schema, errors);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->else_schema) {
        int rc = resolve_schema_refs(ctx, doc, schema->else_schema, errors);
        if (rc < 0) {
            return rc;
        }
    }

    return 0;
}

int oas_ref_resolve_all(oas_ref_ctx_t *ctx, oas_doc_t *doc, oas_error_list_t *errors)
{
    if (!ctx || !doc) {
        return -EINVAL;
    }

    /* Resolve refs in component schemas */
    if (doc->components) {
        for (size_t i = 0; i < doc->components->schemas_count; i++) {
            oas_schema_t *schema = doc->components->schemas[i].schema;
            if (schema) {
                int rc = resolve_schema_refs(ctx, doc, schema, errors);
                if (rc < 0) {
                    return rc;
                }
            }
        }

        /* Resolve refs in component responses */
        for (size_t i = 0; i < doc->components->responses_count; i++) {
            oas_response_t *resp = doc->components->responses[i].response;
            if (!resp) {
                continue;
            }
            for (size_t j = 0; j < resp->content_count; j++) {
                oas_media_type_t *mt = resp->content[j].value;
                if (mt && mt->schema) {
                    int rc = resolve_schema_refs(ctx, doc, mt->schema, errors);
                    if (rc < 0) {
                        return rc;
                    }
                }
            }
        }

        /* Resolve refs in component parameters */
        for (size_t i = 0; i < doc->components->parameters_count; i++) {
            oas_parameter_t *p = doc->components->parameters[i].parameter;
            if (p && p->schema) {
                int rc = resolve_schema_refs(ctx, doc, p->schema, errors);
                if (rc < 0) {
                    return rc;
                }
            }
        }

        /* Resolve refs in component request bodies */
        for (size_t i = 0; i < doc->components->request_bodies_count; i++) {
            oas_request_body_t *rb = doc->components->request_bodies[i].request_body;
            if (!rb) {
                continue;
            }
            for (size_t j = 0; j < rb->content_count; j++) {
                oas_media_type_t *mt = rb->content[j].value;
                if (mt && mt->schema) {
                    int rc = resolve_schema_refs(ctx, doc, mt->schema, errors);
                    if (rc < 0) {
                        return rc;
                    }
                }
            }
        }

        /* Resolve refs in component headers */
        for (size_t i = 0; i < doc->components->headers_count; i++) {
            oas_parameter_t *h = doc->components->headers[i].header;
            if (h && h->schema) {
                int rc = resolve_schema_refs(ctx, doc, h->schema, errors);
                if (rc < 0) {
                    return rc;
                }
            }
        }
    }

    /* Resolve refs in path operation schemas */
    for (size_t i = 0; i < doc->paths_count; i++) {
        oas_path_item_t *pi = doc->paths[i].item;
        if (!pi) {
            continue;
        }

        /* Path-level parameters */
        for (size_t k = 0; k < pi->parameters_count; k++) {
            if (pi->parameters[k] && pi->parameters[k]->schema) {
                int rc = resolve_schema_refs(ctx, doc, pi->parameters[k]->schema, errors);
                if (rc < 0) {
                    return rc;
                }
            }
        }

        oas_operation_t *ops[] = {pi->get,   pi->post, pi->put,    pi->delete_,
                                  pi->patch, pi->head, pi->options};
        for (size_t j = 0; j < sizeof(ops) / sizeof(ops[0]); j++) {
            oas_operation_t *op = ops[j];
            if (!op) {
                continue;
            }

            /* Parameters */
            for (size_t k = 0; k < op->parameters_count; k++) {
                if (op->parameters[k] && op->parameters[k]->schema) {
                    int rc = resolve_schema_refs(ctx, doc, op->parameters[k]->schema, errors);
                    if (rc < 0) {
                        return rc;
                    }
                }
            }

            /* Request body */
            if (op->request_body) {
                for (size_t k = 0; k < op->request_body->content_count; k++) {
                    oas_media_type_t *mt = op->request_body->content[k].value;
                    if (mt && mt->schema) {
                        int rc = resolve_schema_refs(ctx, doc, mt->schema, errors);
                        if (rc < 0) {
                            return rc;
                        }
                    }
                }
            }

            /* Responses */
            for (size_t k = 0; k < op->responses_count; k++) {
                oas_response_t *resp = op->responses[k].response;
                if (!resp) {
                    continue;
                }
                for (size_t m = 0; m < resp->content_count; m++) {
                    oas_media_type_t *mt = resp->content[m].value;
                    if (mt && mt->schema) {
                        int rc = resolve_schema_refs(ctx, doc, mt->schema, errors);
                        if (rc < 0) {
                            return rc;
                        }
                    }
                }
            }
        }
    }

    return 0;
}
