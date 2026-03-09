# Sprint 11: OAS Core Coverage — Components, Security, Server Variables

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Parse the remaining core OAS objects from JSON — security requirements, security schemes, server variables, and all component types ($ref targets). This brings parse coverage from 58% to ~80% of core objects.

**Architecture:** Extend existing parser functions in `oas_doc_parser.c`. Add struct fields to `oas_doc.h` for missing component types. No new modules — purely additive to model + parser layers.

**Tech Stack:** C23, yyjson, Unity 2.6.1

**Skills:** `@modern-c23`, `@liboas-architecture`, `@rfc-reference`

**Build/test:**
```bash
podman run --rm --security-opt seccomp=unconfined \
  --env-file .env -v $(pwd):/workspace:Z \
  localhost/liboas-dev:latest bash -c "cd /workspace && \
  cmake --preset clang-debug && cmake --build --preset clang-debug && \
  ctest --preset clang-debug --output-on-failure"
```

---

## Task 11.1: Parse Security Requirements (root + operation level)

**Files:**
- Modify: `include/liboas/oas_doc.h` — add `oas_security_req_t` type, fields on `oas_doc_t` and `oas_operation_t`
- Modify: `src/parser/oas_doc_parser.c` — parse `security` arrays
- Modify: `src/emitter/oas_emitter.c` — emit parsed security (currently only builder-populated)
- Modify: `tests/unit/test_doc_parser.c` — add parse tests

**Types to add in `oas_doc.h`:**
```c
typedef struct {
    const char *scheme_name;     /**< Security scheme name (key in security req object) */
    const char **scopes;         /**< OAuth2 scopes (array of strings) */
    size_t scopes_count;
} oas_security_req_entry_t;

typedef struct {
    oas_security_req_entry_t *entries;
    size_t entries_count;
} oas_security_req_t;
```

Add to `oas_doc_t`:
```c
    oas_security_req_t **security;
    size_t security_count;
```

Add to `oas_operation_t`:
```c
    oas_security_req_t **security;
    size_t security_count;
```

**Parser implementation in `oas_doc_parser.c`:**
```c
static oas_security_req_t *parse_security_req(oas_arena_t *arena, yyjson_val *obj)
{
    if (!obj || !yyjson_is_obj(obj)) {
        return nullptr;
    }
    size_t n = yyjson_obj_size(obj);
    oas_security_req_t *req = oas_arena_alloc(arena, sizeof(*req), _Alignof(*req));
    if (!req) {
        return nullptr;
    }
    memset(req, 0, sizeof(*req));

    if (n == 0) {
        return req;
    }

    req->entries = oas_arena_alloc(arena, n * sizeof(*req->entries), _Alignof(*req->entries));
    if (!req->entries) {
        return nullptr;
    }
    req->entries_count = n;

    yyjson_val *key, *val;
    size_t idx, max;
    yyjson_obj_foreach(obj, idx, max, key, val) {
        req->entries[idx].scheme_name = yyjson_get_str(key);
        if (yyjson_is_arr(val)) {
            size_t sc = yyjson_arr_size(val);
            if (sc > 0) {
                req->entries[idx].scopes =
                    oas_arena_alloc(arena, sc * sizeof(const char *), _Alignof(const char *));
                if (req->entries[idx].scopes) {
                    req->entries[idx].scopes_count = sc;
                    yyjson_val *scope;
                    size_t si, sm;
                    yyjson_arr_foreach(val, si, sm, scope) {
                        req->entries[idx].scopes[si] =
                            yyjson_is_str(scope) ? yyjson_get_str(scope) : nullptr;
                    }
                }
            }
        }
    }
    return req;
}

static void parse_security_array(oas_arena_t *arena, yyjson_val *arr,
                                  oas_security_req_t ***out, size_t *out_count)
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
    size_t idx, max;
    yyjson_arr_foreach(arr, idx, max, item) {
        (*out)[idx] = parse_security_req(arena, item);
    }
}
```

Call in `oas_doc_parse_ex()`:
```c
    /* security (root level) */
    yyjson_val *security = yyjson_obj_get(jdoc.root, "security");
    if (security) {
        parse_security_array(arena, security, &doc->security, &doc->security_count);
    }
```

Call in `parse_operation()`:
```c
    /* security (operation level) */
    v = yyjson_obj_get(obj, "security");
    if (v) {
        parse_security_array(arena, v, &op->security, &op->security_count);
    }
```

**Tests (4):**
- `test_parse_root_security` — doc with `security: [{bearerAuth: []}]`
- `test_parse_security_with_scopes` — `{oauth2: ["read:pets", "write:pets"]}`
- `test_parse_operation_security` — operation-level override
- `test_parse_empty_security` — `security: [{}]` (anonymous)

**Commit:** `feat: parse security requirements at root and operation levels (4 tests)`

---

## Task 11.2: Parse Security Schemes in Components

**Files:**
- Modify: `include/liboas/oas_doc.h` — add `oas_security_scheme_t`, extend `oas_components_t`
- Modify: `src/parser/oas_doc_parser.c` — parse `components.securitySchemes`
- Modify: `tests/unit/test_doc_parser.c`

**Types to add:**
```c
typedef struct {
    const char *authorization_url;
    const char *token_url;
    const char *refresh_url;
    const char **scope_names;
    const char **scope_descriptions;
    size_t scopes_count;
} oas_oauth_flow_t;

typedef struct {
    oas_oauth_flow_t *implicit;
    oas_oauth_flow_t *password;
    oas_oauth_flow_t *client_credentials;
    oas_oauth_flow_t *authorization_code;
} oas_oauth_flows_t;

typedef struct {
    const char *type;          /**< apiKey, http, oauth2, openIdConnect, mutualTLS */
    const char *description;
    const char *name;          /**< apiKey: header/query/cookie name */
    const char *in;            /**< apiKey: query, header, cookie */
    const char *scheme;        /**< http: bearer, basic, etc. */
    const char *bearer_format; /**< http bearer: JWT, etc. */
    const char *open_id_connect_url;
    oas_oauth_flows_t *flows;
} oas_security_scheme_t;

typedef struct {
    const char *name;
    oas_security_scheme_t *scheme;
} oas_security_scheme_entry_t;
```

Add to `oas_components_t`:
```c
    oas_security_scheme_entry_t *security_schemes;
    size_t security_schemes_count;
```

**Tests (4):**
- `test_parse_security_scheme_apikey` — `{type: apiKey, name: X-API-Key, in: header}`
- `test_parse_security_scheme_http_bearer` — `{type: http, scheme: bearer, bearerFormat: JWT}`
- `test_parse_security_scheme_oauth2` — with flows and scopes
- `test_parse_security_scheme_openid` — `{type: openIdConnect, openIdConnectUrl: ...}`

**Commit:** `feat: parse security schemes from components (4 tests)`

---

## Task 11.3: Parse Server Variables

**Files:**
- Modify: `src/parser/oas_doc_parser.c` — extend `parse_server()` to parse `variables`
- Modify: `tests/unit/test_doc_parser.c`

**Implementation:** `oas_server_t` already has `variables`, `variables_count`, and `oas_server_var_t` is defined. Just need to parse the JSON into these existing structs.

```c
static void parse_server_variables(oas_arena_t *arena, yyjson_val *obj, oas_server_t *srv)
{
    if (!obj || !yyjson_is_obj(obj)) {
        return;
    }
    size_t n = yyjson_obj_size(obj);
    if (n == 0) {
        return;
    }
    srv->variables = oas_arena_alloc(arena, n * sizeof(*srv->variables),
                                      _Alignof(oas_server_var_t));
    if (!srv->variables) {
        return;
    }
    srv->variables_count = n;

    yyjson_val *key, *val;
    size_t idx, max;
    yyjson_obj_foreach(obj, idx, max, key, val) {
        srv->variables[idx].name = yyjson_get_str(key);
        if (yyjson_is_obj(val)) {
            yyjson_val *v;
            v = yyjson_obj_get(val, "default");
            if (v && yyjson_is_str(v)) {
                srv->variables[idx].default_value = yyjson_get_str(v);
            }
            v = yyjson_obj_get(val, "description");
            if (v && yyjson_is_str(v)) {
                srv->variables[idx].description = yyjson_get_str(v);
            }
            v = yyjson_obj_get(val, "enum");
            if (v && yyjson_is_arr(v)) {
                size_t ec = yyjson_arr_size(v);
                if (ec > 0) {
                    srv->variables[idx].enum_values =
                        oas_arena_alloc(arena, ec * sizeof(const char *), _Alignof(const char *));
                    if (srv->variables[idx].enum_values) {
                        srv->variables[idx].enum_count = ec;
                        yyjson_val *ev;
                        size_t ei, em;
                        yyjson_arr_foreach(v, ei, em, ev) {
                            srv->variables[idx].enum_values[ei] =
                                yyjson_is_str(ev) ? yyjson_get_str(ev) : nullptr;
                        }
                    }
                }
            }
        }
    }
}
```

Call in `parse_server()`:
```c
    v = yyjson_obj_get(obj, "variables");
    if (v) {
        parse_server_variables(arena, v, srv);
    }
```

**Tests (3):**
- `test_parse_server_variables` — server with `{port: {default: "8080", enum: ["80", "8080"]}}`
- `test_parse_server_variables_description` — variable with description
- `test_parse_server_no_variables` — server without variables (backward compat)

**Commit:** `feat: parse server variables from JSON (3 tests)`

---

## Task 11.4: Parse Component Responses, Parameters, RequestBodies, Headers

**Files:**
- Modify: `include/liboas/oas_doc.h` — add entry types and fields to `oas_components_t`
- Modify: `src/parser/oas_doc_parser.c` — parse remaining component maps
- Modify: `src/emitter/oas_emitter.c` — emit new component types
- Modify: `tests/unit/test_doc_parser.c`

**Types to add in `oas_doc.h`:**
```c
typedef struct {
    const char *name;
    oas_response_t *response;
} oas_response_component_entry_t;

typedef struct {
    const char *name;
    oas_parameter_t *parameter;
} oas_parameter_component_entry_t;

typedef struct {
    const char *name;
    oas_request_body_t *request_body;
} oas_request_body_component_entry_t;

typedef struct {
    const char *name;
    oas_parameter_t *header; /**< Header Object is a subset of Parameter Object */
} oas_header_component_entry_t;
```

Add to `oas_components_t`:
```c
    oas_response_component_entry_t *responses;
    size_t responses_count;
    oas_parameter_component_entry_t *parameters;
    size_t parameters_count;
    oas_request_body_component_entry_t *request_bodies;
    size_t request_bodies_count;
    oas_header_component_entry_t *headers;
    size_t headers_count;
```

**Parser:** Reuse existing `parse_parameter()`, `parse_content()` for content in responses/requestBodies.

**Tests (4):**
- `test_parse_component_responses` — `components.responses.NotFound`
- `test_parse_component_parameters` — `components.parameters.PageSize`
- `test_parse_component_request_bodies` — `components.requestBodies.PetBody`
- `test_parse_component_headers` — `components.headers.X-Rate-Limit`

**Commit:** `feat: parse component responses, parameters, requestBodies, headers (4 tests)`

---

## Task 11.5: $ref Resolution for New Component Types

**Files:**
- Modify: `src/parser/oas_ref.c` — extend `oas_ref_resolve_all()` to resolve `$ref` in responses, parameters, requestBodies, headers
- Modify: `src/parser/oas_ref.h` — if new API needed
- Modify: `tests/unit/test_ref.c` — add resolution tests for new types

**Implementation:** Currently `oas_ref_resolve_all()` only resolves `$ref` in schemas. Extend to walk responses, parameters, requestBodies that may have `$ref`.

Need to add `ref`/`ref_resolved` fields to `oas_response_t`, `oas_parameter_t`, `oas_request_body_t` if not already present, OR resolve at parse time by looking up component by name.

**Tests (4):**
- `test_ref_resolve_response` — `$ref: "#/components/responses/NotFound"`
- `test_ref_resolve_parameter` — `$ref: "#/components/parameters/PageSize"`
- `test_ref_resolve_request_body` — `$ref: "#/components/requestBodies/PetBody"`
- `test_ref_resolve_header` — `$ref: "#/components/headers/X-Rate-Limit"`

**Commit:** `feat: $ref resolution for component responses, parameters, requestBodies, headers (4 tests)`

---

## Task 11.6: $ref Sibling Keywords (3.1+ Compliance)

**Files:**
- Modify: `src/parser/oas_schema_parser.c:282-287` — remove early return on `$ref`
- Modify: `tests/unit/test_schema_parser.c` — add sibling tests

**Bug:** Lines 282-287 return immediately when `$ref` is found, ignoring all sibling keywords. In OAS 3.1+/JSON Schema 2020-12, `$ref` can coexist with siblings like `description`, `nullable`, composition keywords.

**Fix:**
```c
    /* $ref — store reference URI */
    yyjson_val *ref = yyjson_obj_get(val, "$ref");
    if (ref && yyjson_is_str(ref)) {
        schema->ref = yyjson_get_str(ref);
        /* In 3.1+, $ref can have siblings — continue parsing */
    }
```

Simply remove the `return schema;` after setting `schema->ref`. All subsequent parsing continues normally.

**Tests (4):**
- `test_schema_ref_with_description` — `{"$ref": "...", "description": "override"}`
- `test_schema_ref_with_nullable` — `{"$ref": "...", "nullable": true}`
- `test_schema_ref_only` — `{"$ref": "..."}` still works
- `test_schema_ref_with_allof` — `{"$ref": "...", "allOf": [...]}`

**Commit:** `feat: $ref sibling keywords support for OAS 3.1+ compliance (4 tests)`

---

## Task 11.7: Quality Pipeline

```bash
podman run --rm --security-opt seccomp=unconfined \
  --env-file .env -v $(pwd):/workspace:Z \
  localhost/liboas-dev:latest bash -c "cd /workspace && ./scripts/quality.sh"
```

**Commit:** `chore: quality pipeline clean — Sprint 11`

---

## Summary

| Task | Feature | Tests |
|------|---------|-------|
| 11.1 | Security requirements (root + operation) | 4 |
| 11.2 | Security schemes in components | 4 |
| 11.3 | Server variables | 3 |
| 11.4 | Component responses/parameters/requestBodies/headers | 4 |
| 11.5 | $ref resolution for new component types | 4 |
| 11.6 | $ref sibling keywords (3.1+) | 4 |
| 11.7 | Quality pipeline | 0 |
| **Total** | | **23** |
