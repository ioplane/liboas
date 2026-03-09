#include <liboas/oas_builder.h>

#include <errno.h>
#include <string.h>
#include <strings.h>

/* ── Document builder ──────────────────────────────────────────────────── */

oas_doc_t *oas_doc_build(oas_arena_t *arena, const char *title, const char *version)
{
    if (!arena || !title || !version) {
        return nullptr;
    }

    oas_doc_t *doc = oas_arena_alloc(arena, sizeof(*doc), _Alignof(oas_doc_t));
    if (!doc) {
        return nullptr;
    }
    memset(doc, 0, sizeof(*doc));
    doc->openapi = "3.2.0";

    oas_info_t *info = oas_arena_alloc(arena, sizeof(*info), _Alignof(oas_info_t));
    if (!info) {
        return nullptr;
    }
    memset(info, 0, sizeof(*info));
    info->title = title;
    info->version = version;
    doc->info = info;

    return doc;
}

/* ── Server ────────────────────────────────────────────────────────────── */

int oas_doc_add_server(oas_doc_t *doc, oas_arena_t *arena, const char *url, const char *description)
{
    if (!doc || !arena || !url) {
        return -EINVAL;
    }

    size_t new_count = doc->servers_count + 1;
    oas_server_t **new_arr =
        oas_arena_alloc(arena, new_count * sizeof(*new_arr), _Alignof(oas_server_t *));
    if (!new_arr) {
        return -ENOMEM;
    }

    /* Copy existing pointers */
    if (doc->servers_count > 0) {
        memcpy(new_arr, doc->servers, doc->servers_count * sizeof(*new_arr));
    }

    oas_server_t *srv = oas_arena_alloc(arena, sizeof(*srv), _Alignof(oas_server_t));
    if (!srv) {
        return -ENOMEM;
    }
    memset(srv, 0, sizeof(*srv));
    srv->url = url;
    srv->description = description;

    new_arr[doc->servers_count] = srv;
    doc->servers = new_arr;
    doc->servers_count = new_count;
    return 0;
}

/* ── Components ────────────────────────────────────────────────────────── */

int oas_doc_add_component_schema(oas_doc_t *doc, oas_arena_t *arena, const char *name,
                                 oas_schema_t *schema)
{
    if (!doc || !arena || !name || !schema) {
        return -EINVAL;
    }

    /* Ensure components exists */
    if (!doc->components) {
        doc->components =
            oas_arena_alloc(arena, sizeof(*doc->components), _Alignof(oas_components_t));
        if (!doc->components) {
            return -ENOMEM;
        }
        memset(doc->components, 0, sizeof(*doc->components));
    }

    size_t new_count = doc->components->schemas_count + 1;
    oas_schema_entry_t *new_arr =
        oas_arena_alloc(arena, new_count * sizeof(*new_arr), _Alignof(oas_schema_entry_t));
    if (!new_arr) {
        return -ENOMEM;
    }

    if (doc->components->schemas_count > 0) {
        memcpy(new_arr, doc->components->schemas,
               doc->components->schemas_count * sizeof(*new_arr));
    }

    new_arr[doc->components->schemas_count].name = name;
    new_arr[doc->components->schemas_count].schema = schema;
    doc->components->schemas = new_arr;
    doc->components->schemas_count = new_count;
    return 0;
}

/* ── Path operations ───────────────────────────────────────────────────── */

static oas_operation_t *build_operation(oas_arena_t *arena, const oas_op_builder_t *op)
{
    oas_operation_t *oper = oas_arena_alloc(arena, sizeof(*oper), _Alignof(oas_operation_t));
    if (!oper) {
        return nullptr;
    }
    memset(oper, 0, sizeof(*oper));

    oper->summary = op->summary;
    oper->description = op->description;
    oper->operation_id = op->operation_id;

    /* Tag */
    if (op->tag) {
        const char **tags = oas_arena_alloc(arena, sizeof(*tags), _Alignof(const char *));
        if (!tags) {
            return nullptr;
        }
        tags[0] = op->tag;
        oper->tags = tags;
        oper->tags_count = 1;
    }

    /* Parameters */
    if (op->params) {
        size_t param_count = 0;
        for (const oas_param_builder_t *p = op->params; p->name; p++) {
            param_count++;
        }

        if (param_count > 0) {
            oas_parameter_t **params =
                oas_arena_alloc(arena, param_count * sizeof(*params), _Alignof(oas_parameter_t *));
            if (!params) {
                return nullptr;
            }

            for (size_t i = 0; i < param_count; i++) {
                oas_parameter_t *param =
                    oas_arena_alloc(arena, sizeof(*param), _Alignof(oas_parameter_t));
                if (!param) {
                    return nullptr;
                }
                memset(param, 0, sizeof(*param));
                param->name = op->params[i].name;
                param->in = op->params[i].in;
                param->description = op->params[i].description;
                param->required = op->params[i].required;
                param->schema = op->params[i].schema;
                params[i] = param;
            }
            oper->parameters = params;
            oper->parameters_count = param_count;
        }
    }

    /* Request body */
    if (op->request_body) {
        oas_request_body_t *rb = oas_arena_alloc(arena, sizeof(*rb), _Alignof(oas_request_body_t));
        if (!rb) {
            return nullptr;
        }
        memset(rb, 0, sizeof(*rb));
        rb->required = op->request_body_required;

        const char *ct = op->request_content_type ? op->request_content_type : "application/json";

        oas_media_type_t *mt = oas_arena_alloc(arena, sizeof(*mt), _Alignof(oas_media_type_t));
        if (!mt) {
            return nullptr;
        }
        mt->media_type_name = ct;
        mt->schema = op->request_body;

        oas_media_type_entry_t *entry =
            oas_arena_alloc(arena, sizeof(*entry), _Alignof(oas_media_type_entry_t));
        if (!entry) {
            return nullptr;
        }
        entry->key = ct;
        entry->value = mt;

        rb->content = entry;
        rb->content_count = 1;
        oper->request_body = rb;
    }

    /* Responses */
    if (op->responses) {
        size_t resp_count = 0;
        for (const oas_response_builder_t *r = op->responses; r->status != 0; r++) {
            resp_count++;
        }

        if (resp_count > 0) {
            oas_response_entry_t *entries = oas_arena_alloc(arena, resp_count * sizeof(*entries),
                                                            _Alignof(oas_response_entry_t));
            if (!entries) {
                return nullptr;
            }

            for (size_t i = 0; i < resp_count; i++) {
                /* Format status code as string */
                char *status_str = oas_arena_alloc(arena, 4, 1);
                if (!status_str) {
                    return nullptr;
                }
                (void)snprintf(status_str, 4, "%d", op->responses[i].status);

                oas_response_t *resp =
                    oas_arena_alloc(arena, sizeof(*resp), _Alignof(oas_response_t));
                if (!resp) {
                    return nullptr;
                }
                memset(resp, 0, sizeof(*resp));
                resp->description = op->responses[i].description;

                if (op->responses[i].schema) {
                    const char *ct = op->responses[i].content_type ? op->responses[i].content_type
                                                                   : "application/json";

                    oas_media_type_t *mt =
                        oas_arena_alloc(arena, sizeof(*mt), _Alignof(oas_media_type_t));
                    if (!mt) {
                        return nullptr;
                    }
                    mt->media_type_name = ct;
                    mt->schema = op->responses[i].schema;

                    oas_media_type_entry_t *mte =
                        oas_arena_alloc(arena, sizeof(*mte), _Alignof(oas_media_type_entry_t));
                    if (!mte) {
                        return nullptr;
                    }
                    mte->key = ct;
                    mte->value = mt;

                    resp->content = mte;
                    resp->content_count = 1;
                }

                entries[i].status_code = status_str;
                entries[i].response = resp;
            }
            oper->responses = entries;
            oper->responses_count = resp_count;
        }
    }

    return oper;
}

/**
 * Find or create a path entry in the document.
 */
static oas_path_item_t *find_or_create_path(oas_doc_t *doc, oas_arena_t *arena, const char *path)
{
    /* Search existing */
    for (size_t i = 0; i < doc->paths_count; i++) {
        if (strcmp(doc->paths[i].path, path) == 0) {
            return doc->paths[i].item;
        }
    }

    /* Create new path entry */
    size_t new_count = doc->paths_count + 1;
    oas_path_entry_t *new_arr =
        oas_arena_alloc(arena, new_count * sizeof(*new_arr), _Alignof(oas_path_entry_t));
    if (!new_arr) {
        return nullptr;
    }
    if (doc->paths_count > 0) {
        memcpy(new_arr, doc->paths, doc->paths_count * sizeof(*new_arr));
    }

    oas_path_item_t *item = oas_arena_alloc(arena, sizeof(*item), _Alignof(oas_path_item_t));
    if (!item) {
        return nullptr;
    }
    memset(item, 0, sizeof(*item));
    item->path = path;

    new_arr[doc->paths_count].path = path;
    new_arr[doc->paths_count].item = item;
    doc->paths = new_arr;
    doc->paths_count = new_count;
    return item;
}

int oas_doc_add_path_op(oas_doc_t *doc, oas_arena_t *arena, const char *path, const char *method,
                        const oas_op_builder_t *op)
{
    if (!doc || !arena || !path || !method || !op) {
        return -EINVAL;
    }

    oas_path_item_t *item = find_or_create_path(doc, arena, path);
    if (!item) {
        return -ENOMEM;
    }

    oas_operation_t *oper = build_operation(arena, op);
    if (!oper) {
        return -ENOMEM;
    }

    /* Assign to correct method slot */
    if (strcasecmp(method, "get") == 0) {
        item->get = oper;
    } else if (strcasecmp(method, "post") == 0) {
        item->post = oper;
    } else if (strcasecmp(method, "put") == 0) {
        item->put = oper;
    } else if (strcasecmp(method, "delete") == 0) {
        item->delete_ = oper;
    } else if (strcasecmp(method, "patch") == 0) {
        item->patch = oper;
    } else if (strcasecmp(method, "head") == 0) {
        item->head = oper;
    } else if (strcasecmp(method, "options") == 0) {
        item->options = oper;
    } else {
        return -EINVAL;
    }

    return 0;
}
