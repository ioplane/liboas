#include "oas_ref.h"

#include "core/oas_jsonptr.h"
#include "parser/oas_ref_cache.h"
#include "parser/oas_schema_parser.h"
#include "parser/oas_uri.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

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
    oas_ref_cache_t *cache;
    const oas_ref_options_t *options;
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
    ctx->cache = nullptr;
    ctx->options = nullptr;
    return ctx;
}

void oas_ref_ctx_set_options(oas_ref_ctx_t *ctx, const oas_ref_options_t *options,
                             oas_ref_cache_t *cache)
{
    if (!ctx) {
        return;
    }
    ctx->options = options;
    ctx->cache = cache;
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

/**
 * Resolve an external $ref (file path or HTTP URL) to a yyjson root.
 * Uses cache, file loader, HTTP fetcher, or user callback as appropriate.
 */
static int resolve_external_ref(oas_ref_ctx_t *ctx, const char *ref, const oas_uri_t *uri,
                                yyjson_val **doc_root, oas_error_list_t *errors)
{
    const oas_ref_options_t *opts = ctx->options;

    /* Build cache key without fragment — different fragments reference the same document */
    const char *frag = strchr(ref, '#');
    size_t key_len = frag ? (size_t)(frag - ref) : strlen(ref);
    char cache_key[key_len + 1];
    memcpy(cache_key, ref, key_len);
    cache_key[key_len] = '\0';

    /* Check cache before any fetch */
    if (ctx->cache) {
        yyjson_val *cached = oas_ref_cache_get(ctx->cache, cache_key);
        if (cached) {
            *doc_root = cached;
            return 0;
        }
    }

    /* Determine scheme */
    bool is_http = (uri->scheme && strcmp(uri->scheme, "http") == 0);
    bool is_https = (uri->scheme && strcmp(uri->scheme, "https") == 0);
    bool is_remote = is_http || is_https;

    if (is_remote) {
        if (!opts || !opts->allow_remote) {
            if (errors) {
                oas_error_list_add(errors, OAS_ERR_REF, ref, "remote $ref not allowed: %s", ref);
            }
            return -EACCES;
        }

        /* User callback takes priority */
        if (opts->fetch) {
            char *data = nullptr;
            size_t data_len = 0;
            int rc = opts->fetch(opts->fetch_ctx, ref, &data, &data_len);
            if (rc < 0) {
                return rc;
            }

            /* Parse fetched JSON */
            yyjson_doc *doc = yyjson_read(data, data_len, 0);
            free(data);
            if (!doc) {
                if (errors) {
                    oas_error_list_add(errors, OAS_ERR_PARSE, ref,
                                       "JSON parse error in fetched: %s", ref);
                }
                return -EINVAL;
            }

            yyjson_val *root = yyjson_doc_get_root(doc);
            if (!root) {
                yyjson_doc_free(doc);
                return -EINVAL;
            }

            /* Cache the document */
            if (ctx->cache) {
                rc = oas_ref_cache_put(ctx->cache, cache_key, doc, root);
                if (rc < 0) {
                    yyjson_doc_free(doc);
                    return rc;
                }
            }
            *doc_root = root;
            return 0;
        }

        /* Built-in HTTP fetcher (http only) */
        if (is_http) {
            char *data = nullptr;
            size_t data_len = 0;
            int rc = oas_ref_fetch_http(ref, opts->fetch_timeout_ms, opts->max_fetch_size,
                                        opts->max_redirects, &data, &data_len);
            if (rc < 0) {
                if (errors) {
                    oas_error_list_add(errors, OAS_ERR_REF, ref, "HTTP fetch failed: %s", ref);
                }
                return rc;
            }

            yyjson_doc *doc = yyjson_read(data, data_len, 0);
            free(data);
            if (!doc) {
                if (errors) {
                    oas_error_list_add(errors, OAS_ERR_PARSE, ref,
                                       "JSON parse error in fetched: %s", ref);
                }
                return -EINVAL;
            }

            yyjson_val *root = yyjson_doc_get_root(doc);
            if (!root) {
                yyjson_doc_free(doc);
                return -EINVAL;
            }

            if (ctx->cache) {
                rc = oas_ref_cache_put(ctx->cache, cache_key, doc, root);
                if (rc < 0) {
                    yyjson_doc_free(doc);
                    return rc;
                }
            }
            *doc_root = root;
            return 0;
        }

        /* HTTPS without user callback */
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, ref, "HTTPS requires user fetch callback: %s",
                               ref);
        }
        return -ENOTSUP;
    }

    /* File reference: relative path without scheme */
    if (opts && !opts->allow_file) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, ref, "file $ref not allowed: %s", ref);
        }
        return -EACCES;
    }

    /* Use the path from the URI as the file path */
    const char *file_path = uri->path;
    if (!file_path || file_path[0] == '\0') {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, ref, "empty file path in $ref: %s", ref);
        }
        return -EINVAL;
    }

    const char *base_dir = (opts) ? opts->base_dir : nullptr;
    size_t max_size = (opts) ? opts->max_fetch_size : 0;

    return oas_ref_load_file(file_path, base_dir, ctx->cache, max_size, doc_root, errors);
}

int oas_ref_resolve(oas_ref_ctx_t *ctx, const char *ref, yyjson_val **out, oas_error_list_t *errors)
{
    if (!ctx || !ref || !out) {
        return -EINVAL;
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

    /* Parse the ref as a URI to determine its type */
    oas_uri_t uri = {0};
    int rc = oas_uri_parse(ref, ctx->arena, &uri);
    if (rc < 0) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, ref, "invalid $ref URI: %s", ref);
        }
        return rc;
    }

    /* Fragment-only ref: local resolution */
    if (uri.is_fragment_only) {
        const char *pointer = oas_jsonptr_from_ref(ref);
        yyjson_val *val = oas_jsonptr_resolve_ex(ctx->root, pointer, errors);
        if (!val) {
            return -ENOENT;
        }

        /* If the resolved value itself has a $ref, follow the chain */
        if (yyjson_is_obj(val)) {
            yyjson_val *nested_ref = yyjson_obj_get(val, "$ref");
            if (nested_ref && yyjson_is_str(nested_ref)) {
                rc = visited_push(&ctx->visited, ref, ctx->arena);
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

    /* External ref: requires options and cache */
    if (!ctx->options || !ctx->cache) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, ref, "external $ref not supported: %s", ref);
        }
        return -ENOTSUP;
    }

    /* Resolve external document */
    yyjson_val *ext_root = nullptr;
    rc = resolve_external_ref(ctx, ref, &uri, &ext_root, errors);
    if (rc < 0) {
        return rc;
    }

    /* If there is a fragment, resolve it within the external doc */
    if (uri.fragment && uri.fragment[0] != '\0') {
        char frag_ref[1024];
        rc = snprintf(frag_ref, sizeof(frag_ref), "#%s", uri.fragment);
        if (rc < 0 || (size_t)rc >= sizeof(frag_ref)) {
            return -ENAMETOOLONG;
        }
        const char *pointer = oas_jsonptr_from_ref(frag_ref);
        yyjson_val *val = oas_jsonptr_resolve_ex(ext_root, pointer, errors);
        if (!val) {
            return -ENOENT;
        }
        *out = val;
    } else {
        *out = ext_root;
    }

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
