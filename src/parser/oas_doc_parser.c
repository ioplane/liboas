#include <liboas/oas_parser.h>

#include <errno.h>
#include <string.h>

#include "oas_json.h"
#include "oas_ref.h"
#include "oas_schema_parser.h"

static oas_info_t *parse_info(oas_arena_t *arena, yyjson_val *obj,
                              [[maybe_unused]] oas_error_list_t *errors)
{
    if (!obj || !yyjson_is_obj(obj)) {
        return nullptr;
    }

    oas_info_t *info = oas_arena_alloc(arena, sizeof(*info), _Alignof(oas_info_t));
    if (!info) {
        return nullptr;
    }
    memset(info, 0, sizeof(*info));

    yyjson_val *v;
    v = yyjson_obj_get(obj, "title");
    if (v && yyjson_is_str(v)) {
        info->title = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "description");
    if (v && yyjson_is_str(v)) {
        info->description = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "version");
    if (v && yyjson_is_str(v)) {
        info->version = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "termsOfService");
    if (v && yyjson_is_str(v)) {
        info->terms_of_service = yyjson_get_str(v);
    }

    /* contact */
    v = yyjson_obj_get(obj, "contact");
    if (v && yyjson_is_obj(v)) {
        oas_contact_t *c = oas_arena_alloc(arena, sizeof(*c), _Alignof(oas_contact_t));
        if (c) {
            memset(c, 0, sizeof(*c));
            yyjson_val *cv;
            cv = yyjson_obj_get(v, "name");
            if (cv && yyjson_is_str(cv)) {
                c->name = yyjson_get_str(cv);
            }
            cv = yyjson_obj_get(v, "url");
            if (cv && yyjson_is_str(cv)) {
                c->url = yyjson_get_str(cv);
            }
            cv = yyjson_obj_get(v, "email");
            if (cv && yyjson_is_str(cv)) {
                c->email = yyjson_get_str(cv);
            }
            info->contact = c;
        }
    }

    /* license */
    v = yyjson_obj_get(obj, "license");
    if (v && yyjson_is_obj(v)) {
        oas_license_t *lic = oas_arena_alloc(arena, sizeof(*lic), _Alignof(oas_license_t));
        if (lic) {
            memset(lic, 0, sizeof(*lic));
            yyjson_val *lv;
            lv = yyjson_obj_get(v, "name");
            if (lv && yyjson_is_str(lv)) {
                lic->name = yyjson_get_str(lv);
            }
            lv = yyjson_obj_get(v, "url");
            if (lv && yyjson_is_str(lv)) {
                lic->url = yyjson_get_str(lv);
            }
            info->license = lic;
        }
    }

    return info;
}

static oas_server_t *parse_server(oas_arena_t *arena, yyjson_val *obj)
{
    if (!obj || !yyjson_is_obj(obj)) {
        return nullptr;
    }

    oas_server_t *srv = oas_arena_alloc(arena, sizeof(*srv), _Alignof(oas_server_t));
    if (!srv) {
        return nullptr;
    }
    memset(srv, 0, sizeof(*srv));

    yyjson_val *v;
    v = yyjson_obj_get(obj, "url");
    if (v && yyjson_is_str(v)) {
        srv->url = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "description");
    if (v && yyjson_is_str(v)) {
        srv->description = yyjson_get_str(v);
    }

    /* variables */
    v = yyjson_obj_get(obj, "variables");
    if (v && yyjson_is_obj(v)) {
        size_t n = yyjson_obj_size(v);
        if (n > 0) {
            srv->variables =
                oas_arena_alloc(arena, n * sizeof(*srv->variables), _Alignof(oas_server_var_t *));
            if (srv->variables) {
                srv->variables_count = n;
                yyjson_val *vk;
                yyjson_val *vv;
                size_t vi;
                size_t vm;
                yyjson_obj_foreach(v, vi, vm, vk, vv)
                {
                    oas_server_var_t *sv =
                        oas_arena_alloc(arena, sizeof(*sv), _Alignof(oas_server_var_t));
                    if (!sv) {
                        continue;
                    }
                    memset(sv, 0, sizeof(*sv));
                    sv->name = yyjson_get_str(vk);
                    if (yyjson_is_obj(vv)) {
                        yyjson_val *dv;
                        dv = yyjson_obj_get(vv, "default");
                        if (dv && yyjson_is_str(dv)) {
                            sv->default_value = yyjson_get_str(dv);
                        }
                        dv = yyjson_obj_get(vv, "description");
                        if (dv && yyjson_is_str(dv)) {
                            sv->description = yyjson_get_str(dv);
                        }
                        dv = yyjson_obj_get(vv, "enum");
                        if (dv && yyjson_is_arr(dv)) {
                            size_t ec = yyjson_arr_size(dv);
                            if (ec > 0) {
                                sv->enum_values = oas_arena_alloc(arena, ec * sizeof(const char *),
                                                                  _Alignof(const char *));
                                if (sv->enum_values) {
                                    sv->enum_count = ec;
                                    yyjson_val *ev;
                                    size_t ei;
                                    size_t em;
                                    yyjson_arr_foreach(dv, ei, em, ev)
                                    {
                                        sv->enum_values[ei] = yyjson_is_str(ev) ? yyjson_get_str(ev)
                                                                                : nullptr;
                                    }
                                }
                            }
                        }
                    }
                    srv->variables[vi] = sv;
                }
            }
        }
    }

    return srv;
}

/* ── Security requirement parsing ────────────────────────────────────── */

static void parse_security_array(oas_arena_t *arena, yyjson_val *arr, oas_security_req_t ***out,
                                 size_t *out_count)
{
    *out = nullptr;
    *out_count = 0;
    if (!arr || !yyjson_is_arr(arr)) {
        return;
    }
    size_t count = yyjson_arr_size(arr);
    if (count == 0) {
        return;
    }
    *out = oas_arena_alloc(arena, count * sizeof(oas_security_req_t *),
                           _Alignof(oas_security_req_t *));
    if (!*out) {
        return;
    }
    *out_count = count;
    yyjson_val *item;
    size_t idx;
    size_t max;
    yyjson_arr_foreach(arr, idx, max, item)
    {
        if (!yyjson_is_obj(item) || yyjson_obj_size(item) == 0) {
            /* Empty object = anonymous access */
            (*out)[idx] = nullptr;
            continue;
        }
        /* Take the first key-value pair as the scheme */
        yyjson_obj_iter iter = yyjson_obj_iter_with(item);
        yyjson_val *key = yyjson_obj_iter_next(&iter);
        yyjson_val *val = yyjson_obj_iter_get_val(key);
        oas_security_req_t *req =
            oas_arena_alloc(arena, sizeof(*req), _Alignof(oas_security_req_t));
        if (!req) {
            (*out)[idx] = nullptr;
            continue;
        }
        memset(req, 0, sizeof(*req));
        req->name = yyjson_get_str(key);
        if (val && yyjson_is_arr(val)) {
            size_t sc = yyjson_arr_size(val);
            if (sc > 0) {
                req->scopes =
                    oas_arena_alloc(arena, sc * sizeof(const char *), _Alignof(const char *));
                if (req->scopes) {
                    req->scopes_count = sc;
                    yyjson_val *scope;
                    size_t si;
                    size_t sm;
                    yyjson_arr_foreach(val, si, sm, scope)
                    {
                        req->scopes[si] = yyjson_is_str(scope) ? yyjson_get_str(scope) : nullptr;
                    }
                }
            }
        }
        (*out)[idx] = req;
    }
}

/* ── Security scheme parsing ────────────────────────────────────────── */

static oas_oauth_flow_t *parse_oauth_flow(oas_arena_t *arena, yyjson_val *obj)
{
    if (!obj || !yyjson_is_obj(obj)) {
        return nullptr;
    }
    oas_oauth_flow_t *flow = oas_arena_alloc(arena, sizeof(*flow), _Alignof(oas_oauth_flow_t));
    if (!flow) {
        return nullptr;
    }
    memset(flow, 0, sizeof(*flow));
    yyjson_val *v;
    v = yyjson_obj_get(obj, "authorizationUrl");
    if (v && yyjson_is_str(v)) {
        flow->authorization_url = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "tokenUrl");
    if (v && yyjson_is_str(v)) {
        flow->token_url = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "refreshUrl");
    if (v && yyjson_is_str(v)) {
        flow->refresh_url = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "scopes");
    if (v && yyjson_is_obj(v)) {
        size_t n = yyjson_obj_size(v);
        if (n > 0) {
            flow->scope_names =
                oas_arena_alloc(arena, n * sizeof(const char *), _Alignof(const char *));
            flow->scope_descriptions =
                oas_arena_alloc(arena, n * sizeof(const char *), _Alignof(const char *));
            if (flow->scope_names && flow->scope_descriptions) {
                flow->scopes_count = n;
                yyjson_val *sk;
                yyjson_val *sv;
                size_t si;
                size_t sm;
                yyjson_obj_foreach(v, si, sm, sk, sv)
                {
                    flow->scope_names[si] = yyjson_get_str(sk);
                    flow->scope_descriptions[si] = yyjson_is_str(sv) ? yyjson_get_str(sv) : nullptr;
                }
            }
        }
    }
    return flow;
}

static oas_security_scheme_t *parse_security_scheme(oas_arena_t *arena, yyjson_val *obj)
{
    if (!obj || !yyjson_is_obj(obj)) {
        return nullptr;
    }
    oas_security_scheme_t *ss =
        oas_arena_alloc(arena, sizeof(*ss), _Alignof(oas_security_scheme_t));
    if (!ss) {
        return nullptr;
    }
    memset(ss, 0, sizeof(*ss));
    yyjson_val *v;
    v = yyjson_obj_get(obj, "type");
    if (v && yyjson_is_str(v)) {
        ss->type = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "description");
    if (v && yyjson_is_str(v)) {
        ss->description = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "name");
    if (v && yyjson_is_str(v)) {
        ss->name = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "in");
    if (v && yyjson_is_str(v)) {
        ss->in = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "scheme");
    if (v && yyjson_is_str(v)) {
        ss->scheme = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "bearerFormat");
    if (v && yyjson_is_str(v)) {
        ss->bearer_format = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "openIdConnectUrl");
    if (v && yyjson_is_str(v)) {
        ss->open_id_connect_url = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "flows");
    if (v && yyjson_is_obj(v)) {
        oas_oauth_flows_t *flows =
            oas_arena_alloc(arena, sizeof(*flows), _Alignof(oas_oauth_flows_t));
        if (flows) {
            memset(flows, 0, sizeof(*flows));
            flows->implicit = parse_oauth_flow(arena, yyjson_obj_get(v, "implicit"));
            flows->password = parse_oauth_flow(arena, yyjson_obj_get(v, "password"));
            flows->client_credentials =
                parse_oauth_flow(arena, yyjson_obj_get(v, "clientCredentials"));
            flows->authorization_code =
                parse_oauth_flow(arena, yyjson_obj_get(v, "authorizationCode"));
            ss->flows = flows;
        }
    }
    return ss;
}

static oas_parameter_t *parse_parameter(oas_arena_t *arena, yyjson_val *obj,
                                        oas_error_list_t *errors)
{
    if (!obj || !yyjson_is_obj(obj)) {
        return nullptr;
    }

    oas_parameter_t *p = oas_arena_alloc(arena, sizeof(*p), _Alignof(oas_parameter_t));
    if (!p) {
        return nullptr;
    }
    memset(p, 0, sizeof(*p));

    yyjson_val *v;
    v = yyjson_obj_get(obj, "name");
    if (v && yyjson_is_str(v)) {
        p->name = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "in");
    if (v && yyjson_is_str(v)) {
        p->in = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "description");
    if (v && yyjson_is_str(v)) {
        p->description = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "required");
    if (v && yyjson_is_bool(v)) {
        p->required = yyjson_get_bool(v);
    }
    v = yyjson_obj_get(obj, "deprecated");
    if (v && yyjson_is_bool(v)) {
        p->deprecated = yyjson_get_bool(v);
    }
    v = yyjson_obj_get(obj, "schema");
    if (v && yyjson_is_obj(v)) {
        p->schema = oas_schema_parse(arena, v, errors);
    }
    return p;
}

static oas_media_type_entry_t *parse_content(oas_arena_t *arena, yyjson_val *obj, size_t *count,
                                             oas_error_list_t *errors)
{
    *count = 0;
    if (!obj || !yyjson_is_obj(obj)) {
        return nullptr;
    }

    size_t n = yyjson_obj_size(obj);
    if (n == 0) {
        return nullptr;
    }

    oas_media_type_entry_t *entries = oas_arena_alloc(arena, sizeof(oas_media_type_entry_t) * n,
                                                      _Alignof(oas_media_type_entry_t));
    if (!entries) {
        return nullptr;
    }

    yyjson_val *key;
    yyjson_val *val;
    size_t idx;
    size_t max;
    yyjson_obj_foreach(obj, idx, max, key, val)
    {
        entries[idx].key = yyjson_get_str(key);
        oas_media_type_t *mt = oas_arena_alloc(arena, sizeof(*mt), _Alignof(oas_media_type_t));
        if (mt) {
            memset(mt, 0, sizeof(*mt));
            mt->media_type_name = yyjson_get_str(key);
            yyjson_val *schema_val = yyjson_obj_get(val, "schema");
            if (schema_val && yyjson_is_obj(schema_val)) {
                mt->schema = oas_schema_parse(arena, schema_val, errors);
            }
        }
        entries[idx].value = mt;
    }
    *count = n;
    return entries;
}

static void parse_responses(oas_arena_t *arena, yyjson_val *obj, oas_operation_t *op,
                            oas_error_list_t *errors)
{
    if (!obj || !yyjson_is_obj(obj)) {
        return;
    }

    size_t n = yyjson_obj_size(obj);
    if (n == 0) {
        return;
    }

    op->responses =
        oas_arena_alloc(arena, sizeof(oas_response_entry_t) * n, _Alignof(oas_response_entry_t));
    if (!op->responses) {
        return;
    }

    yyjson_val *key;
    yyjson_val *val;
    size_t idx;
    size_t max;
    yyjson_obj_foreach(obj, idx, max, key, val)
    {
        op->responses[idx].status_code = yyjson_get_str(key);
        oas_response_t *resp = oas_arena_alloc(arena, sizeof(*resp), _Alignof(oas_response_t));
        if (resp) {
            memset(resp, 0, sizeof(*resp));
            yyjson_val *desc = yyjson_obj_get(val, "description");
            if (desc && yyjson_is_str(desc)) {
                resp->description = yyjson_get_str(desc);
            }
            yyjson_val *content = yyjson_obj_get(val, "content");
            if (content) {
                resp->content = parse_content(arena, content, &resp->content_count, errors);
            }
        }
        op->responses[idx].response = resp;
    }
    op->responses_count = n;
}

static oas_operation_t *parse_operation(oas_arena_t *arena, yyjson_val *obj,
                                        oas_error_list_t *errors)
{
    if (!obj || !yyjson_is_obj(obj)) {
        return nullptr;
    }

    oas_operation_t *op = oas_arena_alloc(arena, sizeof(*op), _Alignof(oas_operation_t));
    if (!op) {
        return nullptr;
    }
    memset(op, 0, sizeof(*op));

    yyjson_val *v;
    v = yyjson_obj_get(obj, "operationId");
    if (v && yyjson_is_str(v)) {
        op->operation_id = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "summary");
    if (v && yyjson_is_str(v)) {
        op->summary = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "description");
    if (v && yyjson_is_str(v)) {
        op->description = yyjson_get_str(v);
    }
    v = yyjson_obj_get(obj, "deprecated");
    if (v && yyjson_is_bool(v)) {
        op->deprecated = yyjson_get_bool(v);
    }

    /* tags */
    v = yyjson_obj_get(obj, "tags");
    if (v && yyjson_is_arr(v)) {
        size_t count = yyjson_arr_size(v);
        if (count > 0) {
            op->tags = oas_arena_alloc(arena, sizeof(const char *) * count, _Alignof(const char *));
            if (op->tags) {
                op->tags_count = count;
                yyjson_val *tag;
                size_t ti;
                size_t tm;
                yyjson_arr_foreach(v, ti, tm, tag)
                {
                    op->tags[ti] = yyjson_is_str(tag) ? yyjson_get_str(tag) : nullptr;
                }
            }
        }
    }

    /* parameters */
    v = yyjson_obj_get(obj, "parameters");
    if (v && yyjson_is_arr(v)) {
        size_t count = yyjson_arr_size(v);
        if (count > 0) {
            op->parameters = oas_arena_alloc(arena, sizeof(oas_parameter_t *) * count,
                                             _Alignof(oas_parameter_t *));
            if (op->parameters) {
                op->parameters_count = count;
                yyjson_val *param;
                size_t pi;
                size_t pm;
                yyjson_arr_foreach(v, pi, pm, param)
                {
                    op->parameters[pi] = parse_parameter(arena, param, errors);
                }
            }
        }
    }

    /* requestBody */
    v = yyjson_obj_get(obj, "requestBody");
    if (v && yyjson_is_obj(v)) {
        oas_request_body_t *rb = oas_arena_alloc(arena, sizeof(*rb), _Alignof(oas_request_body_t));
        if (rb) {
            memset(rb, 0, sizeof(*rb));
            yyjson_val *desc = yyjson_obj_get(v, "description");
            if (desc && yyjson_is_str(desc)) {
                rb->description = yyjson_get_str(desc);
            }
            yyjson_val *req = yyjson_obj_get(v, "required");
            if (req && yyjson_is_bool(req)) {
                rb->required = yyjson_get_bool(req);
            }
            yyjson_val *content = yyjson_obj_get(v, "content");
            if (content) {
                rb->content = parse_content(arena, content, &rb->content_count, errors);
            }
            op->request_body = rb;
        }
    }

    /* responses */
    v = yyjson_obj_get(obj, "responses");
    if (v) {
        parse_responses(arena, v, op, errors);
    }

    /* security (operation level) */
    v = yyjson_obj_get(obj, "security");
    if (v) {
        parse_security_array(arena, v, &op->security, &op->security_count);
    }

    return op;
}

static void parse_path_item(oas_arena_t *arena, yyjson_val *obj, oas_path_item_t *pi,
                            oas_error_list_t *errors)
{
    static const struct {
        const char *method;
        size_t offset;
    } methods[] = {
        {"get", offsetof(oas_path_item_t, get)},
        {"post", offsetof(oas_path_item_t, post)},
        {"put", offsetof(oas_path_item_t, put)},
        {"delete", offsetof(oas_path_item_t, delete_)},
        {"patch", offsetof(oas_path_item_t, patch)},
        {"head", offsetof(oas_path_item_t, head)},
        {"options", offsetof(oas_path_item_t, options)},
    };

    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
        yyjson_val *v = yyjson_obj_get(obj, methods[i].method);
        if (v && yyjson_is_obj(v)) {
            oas_operation_t **slot = (oas_operation_t **)((char *)pi + methods[i].offset);
            *slot = parse_operation(arena, v, errors);
        }
    }

    /* path-level parameters */
    yyjson_val *params = yyjson_obj_get(obj, "parameters");
    if (params && yyjson_is_arr(params)) {
        size_t count = yyjson_arr_size(params);
        if (count > 0) {
            pi->parameters = oas_arena_alloc(arena, sizeof(oas_parameter_t *) * count,
                                             _Alignof(oas_parameter_t *));
            if (pi->parameters) {
                pi->parameters_count = count;
                yyjson_val *param;
                size_t idx;
                size_t max;
                yyjson_arr_foreach(params, idx, max, param)
                {
                    pi->parameters[idx] = parse_parameter(arena, param, errors);
                }
            }
        }
    }
}

static void parse_components(oas_arena_t *arena, yyjson_val *obj, oas_doc_t *doc,
                             oas_error_list_t *errors)
{
    if (!obj || !yyjson_is_obj(obj)) {
        return;
    }

    oas_components_t *comp = oas_arena_alloc(arena, sizeof(*comp), _Alignof(oas_components_t));
    if (!comp) {
        return;
    }
    memset(comp, 0, sizeof(*comp));

    /* schemas */
    yyjson_val *schemas = yyjson_obj_get(obj, "schemas");
    if (schemas && yyjson_is_obj(schemas)) {
        size_t n = yyjson_obj_size(schemas);
        if (n > 0) {
            comp->schemas = oas_arena_alloc(arena, sizeof(oas_schema_entry_t) * n,
                                            _Alignof(oas_schema_entry_t));
            if (comp->schemas) {
                comp->schemas_count = n;
                yyjson_val *key;
                yyjson_val *val;
                size_t idx;
                size_t max;
                yyjson_obj_foreach(schemas, idx, max, key, val)
                {
                    comp->schemas[idx].name = yyjson_get_str(key);
                    comp->schemas[idx].schema = oas_schema_parse(arena, val, errors);
                }
            }
        }
    }

    /* securitySchemes */
    yyjson_val *sec_schemes = yyjson_obj_get(obj, "securitySchemes");
    if (sec_schemes && yyjson_is_obj(sec_schemes)) {
        size_t n = yyjson_obj_size(sec_schemes);
        if (n > 0) {
            comp->security_schemes = oas_arena_alloc(arena, sizeof(oas_security_scheme_entry_t) * n,
                                                     _Alignof(oas_security_scheme_entry_t));
            if (comp->security_schemes) {
                comp->security_schemes_count = n;
                yyjson_val *key;
                yyjson_val *val;
                size_t idx;
                size_t max;
                yyjson_obj_foreach(sec_schemes, idx, max, key, val)
                {
                    comp->security_schemes[idx].name = yyjson_get_str(key);
                    comp->security_schemes[idx].scheme = parse_security_scheme(arena, val);
                }
            }
        }
    }

    /* responses */
    yyjson_val *responses = yyjson_obj_get(obj, "responses");
    if (responses && yyjson_is_obj(responses)) {
        size_t n = yyjson_obj_size(responses);
        if (n > 0) {
            comp->responses = oas_arena_alloc(arena, sizeof(oas_response_component_entry_t) * n,
                                              _Alignof(oas_response_component_entry_t));
            if (comp->responses) {
                comp->responses_count = n;
                yyjson_val *key;
                yyjson_val *val;
                size_t idx;
                size_t max;
                yyjson_obj_foreach(responses, idx, max, key, val)
                {
                    comp->responses[idx].name = yyjson_get_str(key);
                    oas_response_t *resp =
                        oas_arena_alloc(arena, sizeof(*resp), _Alignof(oas_response_t));
                    if (resp) {
                        memset(resp, 0, sizeof(*resp));
                        yyjson_val *dv = yyjson_obj_get(val, "description");
                        if (dv && yyjson_is_str(dv)) {
                            resp->description = yyjson_get_str(dv);
                        }
                        yyjson_val *cv = yyjson_obj_get(val, "content");
                        if (cv) {
                            resp->content = parse_content(arena, cv, &resp->content_count, errors);
                        }
                    }
                    comp->responses[idx].response = resp;
                }
            }
        }
    }

    /* parameters */
    yyjson_val *params = yyjson_obj_get(obj, "parameters");
    if (params && yyjson_is_obj(params)) {
        size_t n = yyjson_obj_size(params);
        if (n > 0) {
            comp->parameters = oas_arena_alloc(arena, sizeof(oas_parameter_component_entry_t) * n,
                                               _Alignof(oas_parameter_component_entry_t));
            if (comp->parameters) {
                comp->parameters_count = n;
                yyjson_val *key;
                yyjson_val *val;
                size_t idx;
                size_t max;
                yyjson_obj_foreach(params, idx, max, key, val)
                {
                    comp->parameters[idx].name = yyjson_get_str(key);
                    comp->parameters[idx].parameter = parse_parameter(arena, val, errors);
                }
            }
        }
    }

    /* requestBodies */
    yyjson_val *req_bodies = yyjson_obj_get(obj, "requestBodies");
    if (req_bodies && yyjson_is_obj(req_bodies)) {
        size_t n = yyjson_obj_size(req_bodies);
        if (n > 0) {
            comp->request_bodies = oas_arena_alloc(arena,
                                                   sizeof(oas_request_body_component_entry_t) * n,
                                                   _Alignof(oas_request_body_component_entry_t));
            if (comp->request_bodies) {
                comp->request_bodies_count = n;
                yyjson_val *key;
                yyjson_val *val;
                size_t idx;
                size_t max;
                yyjson_obj_foreach(req_bodies, idx, max, key, val)
                {
                    comp->request_bodies[idx].name = yyjson_get_str(key);
                    oas_request_body_t *rb =
                        oas_arena_alloc(arena, sizeof(*rb), _Alignof(oas_request_body_t));
                    if (rb) {
                        memset(rb, 0, sizeof(*rb));
                        yyjson_val *dv = yyjson_obj_get(val, "description");
                        if (dv && yyjson_is_str(dv)) {
                            rb->description = yyjson_get_str(dv);
                        }
                        yyjson_val *rv = yyjson_obj_get(val, "required");
                        if (rv && yyjson_is_bool(rv)) {
                            rb->required = yyjson_get_bool(rv);
                        }
                        yyjson_val *cv = yyjson_obj_get(val, "content");
                        if (cv) {
                            rb->content = parse_content(arena, cv, &rb->content_count, errors);
                        }
                    }
                    comp->request_bodies[idx].request_body = rb;
                }
            }
        }
    }

    /* headers */
    yyjson_val *headers = yyjson_obj_get(obj, "headers");
    if (headers && yyjson_is_obj(headers)) {
        size_t n = yyjson_obj_size(headers);
        if (n > 0) {
            comp->headers = oas_arena_alloc(arena, sizeof(oas_header_component_entry_t) * n,
                                            _Alignof(oas_header_component_entry_t));
            if (comp->headers) {
                comp->headers_count = n;
                yyjson_val *key;
                yyjson_val *val;
                size_t idx;
                size_t max;
                yyjson_obj_foreach(headers, idx, max, key, val)
                {
                    comp->headers[idx].name = yyjson_get_str(key);
                    /* Header Object is a subset of Parameter — reuse parser */
                    comp->headers[idx].header = parse_parameter(arena, val, errors);
                }
            }
        }
    }

    doc->components = comp;
}

static bool check_openapi_version(const char *version)
{
    if (!version) {
        return false;
    }
    /* Accept 3.x.y */
    return version[0] == '3' && version[1] == '.';
}

oas_doc_t *oas_doc_parse(oas_arena_t *arena, const char *json, size_t len, oas_error_list_t *errors)
{
    if (!arena || !json) {
        return nullptr;
    }

    oas_json_doc_t jdoc = {0};
    int rc = oas_json_parse(json, len, &jdoc, errors);
    if (rc != 0) {
        return nullptr;
    }

    if (!yyjson_is_obj(jdoc.root)) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_PARSE, "/", "root must be an object");
        }
        oas_json_free(&jdoc);
        return nullptr;
    }

    /* openapi version check */
    yyjson_val *ver_val = yyjson_obj_get(jdoc.root, "openapi");
    if (!ver_val || !yyjson_is_str(ver_val)) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REQUIRED, "/openapi",
                               "missing required field: openapi");
        }
        oas_json_free(&jdoc);
        return nullptr;
    }
    const char *ver_str = yyjson_get_str(ver_val);
    if (!check_openapi_version(ver_str)) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_SCHEMA, "/openapi",
                               "unsupported OpenAPI version: %s (expected 3.x)", ver_str);
        }
        oas_json_free(&jdoc);
        return nullptr;
    }

    /* info check */
    yyjson_val *info_val = yyjson_obj_get(jdoc.root, "info");
    if (!info_val || !yyjson_is_obj(info_val)) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REQUIRED, "/info", "missing required field: info");
        }
        oas_json_free(&jdoc);
        return nullptr;
    }

    oas_doc_t *doc = oas_arena_alloc(arena, sizeof(*doc), _Alignof(oas_doc_t));
    if (!doc) {
        oas_json_free(&jdoc);
        return nullptr;
    }
    memset(doc, 0, sizeof(*doc));

    doc->openapi = ver_str;
    doc->info = parse_info(arena, info_val, errors);

    /* servers */
    yyjson_val *servers = yyjson_obj_get(jdoc.root, "servers");
    if (servers && yyjson_is_arr(servers)) {
        size_t count = yyjson_arr_size(servers);
        if (count > 0) {
            doc->servers =
                oas_arena_alloc(arena, sizeof(oas_server_t *) * count, _Alignof(oas_server_t *));
            if (doc->servers) {
                doc->servers_count = count;
                yyjson_val *srv;
                size_t si;
                size_t sm;
                yyjson_arr_foreach(servers, si, sm, srv)
                {
                    doc->servers[si] = parse_server(arena, srv);
                }
            }
        }
    }

    /* paths */
    yyjson_val *paths = yyjson_obj_get(jdoc.root, "paths");
    if (paths && yyjson_is_obj(paths)) {
        size_t n = yyjson_obj_size(paths);
        if (n > 0) {
            doc->paths =
                oas_arena_alloc(arena, sizeof(oas_path_entry_t) * n, _Alignof(oas_path_entry_t));
            if (doc->paths) {
                doc->paths_count = n;
                yyjson_val *key;
                yyjson_val *val;
                size_t idx;
                size_t max;
                yyjson_obj_foreach(paths, idx, max, key, val)
                {
                    doc->paths[idx].path = yyjson_get_str(key);
                    oas_path_item_t *pi =
                        oas_arena_alloc(arena, sizeof(*pi), _Alignof(oas_path_item_t));
                    if (pi) {
                        memset(pi, 0, sizeof(*pi));
                        pi->path = yyjson_get_str(key);
                        parse_path_item(arena, val, pi, errors);
                    }
                    doc->paths[idx].item = pi;
                }
            }
        }
    }

    /* components */
    yyjson_val *components = yyjson_obj_get(jdoc.root, "components");
    if (components) {
        parse_components(arena, components, doc, errors);
    }

    /* security (root level) */
    yyjson_val *security = yyjson_obj_get(jdoc.root, "security");
    if (security) {
        parse_security_array(arena, security, &doc->security, &doc->security_count);
    }

    /* tags */
    yyjson_val *tags = yyjson_obj_get(jdoc.root, "tags");
    if (tags && yyjson_is_arr(tags)) {
        size_t count = yyjson_arr_size(tags);
        if (count > 0) {
            doc->tags = oas_arena_alloc(arena, sizeof(oas_tag_t *) * count, _Alignof(oas_tag_t *));
            if (doc->tags) {
                doc->tags_count = count;
                yyjson_val *tag;
                size_t ti;
                size_t tm;
                yyjson_arr_foreach(tags, ti, tm, tag)
                {
                    oas_tag_t *t = oas_arena_alloc(arena, sizeof(*t), _Alignof(oas_tag_t));
                    if (t) {
                        memset(t, 0, sizeof(*t));
                        yyjson_val *tv;
                        tv = yyjson_obj_get(tag, "name");
                        if (tv && yyjson_is_str(tv)) {
                            t->name = yyjson_get_str(tv);
                        }
                        tv = yyjson_obj_get(tag, "description");
                        if (tv && yyjson_is_str(tv)) {
                            t->description = yyjson_get_str(tv);
                        }
                    }
                    doc->tags[ti] = t;
                }
            }
        }
    }

    /* Store yyjson_doc so it can be freed via oas_doc_free() */
    doc->_json_doc = jdoc.doc;

    /* Resolve all $ref in the document */
    oas_ref_ctx_t *ref_ctx = oas_ref_ctx_create(arena, jdoc.root);
    if (ref_ctx) {
        (void)oas_ref_resolve_all(ref_ctx, doc, errors);
    }

    return doc;
}

oas_doc_t *oas_doc_parse_file(oas_arena_t *arena, const char *path, oas_error_list_t *errors)
{
    if (!arena || !path) {
        return nullptr;
    }

    oas_json_doc_t jdoc = {0};
    int rc = oas_json_parse_file(path, &jdoc, errors);
    if (rc != 0) {
        return nullptr;
    }

    /* Extract the raw JSON to reparse through oas_doc_parse */
    /* Actually we already have root — build doc directly */
    /* Re-serialize isn't needed, let's just inline the same logic */

    if (!yyjson_is_obj(jdoc.root)) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_PARSE, "/", "root must be an object");
        }
        oas_json_free(&jdoc);
        return nullptr;
    }

    /* Reuse the string parse path by reading file content ourselves.
     * But we already have jdoc parsed. Let's build a lightweight wrapper. */

    /* For simplicity, just use yyjson_doc_get_root to get size and re-read. */
    /* Actually, the simplest is to read the file, then call oas_doc_parse. */
    oas_json_free(&jdoc);

    /* Read file contents */
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return nullptr;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        (void)fclose(fp);
        return nullptr;
    }
    long fsize = ftell(fp);
    if (fsize < 0) {
        (void)fclose(fp);
        return nullptr;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        (void)fclose(fp);
        return nullptr;
    }

    size_t size = (size_t)fsize;
    char *buf = malloc(size + 1);
    if (!buf) {
        (void)fclose(fp);
        return nullptr;
    }

    size_t read_bytes = fread(buf, 1, size, fp);
    (void)fclose(fp);

    if (read_bytes != size) {
        free(buf);
        return nullptr;
    }
    buf[size] = '\0';

    oas_doc_t *doc = oas_doc_parse(arena, buf, size, errors);
    free(buf);
    return doc;
}
