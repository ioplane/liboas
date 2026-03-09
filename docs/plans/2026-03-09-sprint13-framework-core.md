# Sprint 13: API Framework Core — Headers, Query Params, Error Responses

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extend request/response validation to cover headers, query parameters, cookies, and produce RFC 9457 Problem Details error responses. This transforms liboas from a body-only validator into a complete HTTP API validation framework.

**Architecture:** Extend `oas_http_request_t` and `oas_http_response_t` with header/query maps. Add query string parser. Add RFC 9457 error formatter. No new modules — extends `oas_validator` and `oas_adapter`.

**Tech Stack:** C23, yyjson (for error JSON), Unity

**Skills:** `@liboas-architecture`, `@rfc-reference`, `@modern-c23`

---

## Task 13.1: Header Map in Request/Response Types

**Files:**
- Modify: `include/liboas/oas_validator.h` — extend `oas_http_request_t`, `oas_http_response_t`
- Modify: `src/validator/oas_request.c` — use headers for validation
- Modify: all test files that construct `oas_http_request_t`/`oas_http_response_t` (add `.headers = nullptr` to maintain compat)

**New types:**
```c
typedef struct {
    const char *name;
    const char *value;
} oas_http_header_t;

typedef struct {
    const char *name;
    const char *value;
} oas_http_query_param_t;
```

Extend `oas_http_request_t`:
```c
typedef struct {
    const char *method;
    const char *path;
    const char *content_type;
    const char *body;
    size_t body_len;
    const oas_http_header_t *headers;       /**< Request headers (nullable) */
    size_t headers_count;
    const oas_http_query_param_t *query;    /**< Query parameters (nullable) */
    size_t query_count;
    const char *query_string;               /**< Raw query string (nullable) */
} oas_http_request_t;
```

Extend `oas_http_response_t`:
```c
typedef struct {
    int status_code;
    const char *content_type;
    const char *body;
    size_t body_len;
    const oas_http_header_t *headers;
    size_t headers_count;
} oas_http_response_t;
```

**Tests (3):**
- `test_request_with_headers` — pass headers, verify accessible
- `test_request_without_headers` — nullptr headers, backward compat
- `test_response_with_headers` — response headers stored

**Commit:** `feat: header and query param maps in HTTP request/response types (3 tests)`

---

## Task 13.2: Header Validation

**Files:**
- Modify: `src/validator/oas_request.c` — validate request headers against parameter schemas
- Modify: `src/compiler/oas_doc_compiler.c` — compile header parameter schemas
- Modify: tests

**Logic:** For each `in: header` parameter in the compiled operation, find the matching header in `req->headers` by name (case-insensitive), parse the value as JSON, and validate against the parameter's compiled schema. If `required=true` and header missing, error.

**Tests (5):**
- `test_validate_header_pass` — `X-Request-ID: "abc"` matches string schema
- `test_validate_header_fail` — header value doesn't match schema
- `test_validate_header_required_missing` — required header absent
- `test_validate_header_optional_missing` — optional header absent, no error
- `test_validate_header_case_insensitive` — `x-request-id` matches `X-Request-ID`

**Commit:** `feat: request header validation against parameter schemas (5 tests)`

---

## Task 13.3: Query Parameter Validation

**Files:**
- Modify: `src/validator/oas_request.c` — validate query params
- Create: `src/core/oas_query.c` — query string parser
- Create: `src/core/oas_query.h`
- Modify: `CMakeLists.txt`
- Modify: tests

**Query string parser:** Simple `key=value&key2=value2` parser. No style/explode yet — just basic form encoding.

```c
typedef struct {
    const char *key;
    const char *value;
} oas_query_pair_t;

[[nodiscard]] int oas_query_parse(oas_arena_t *arena, const char *query_string,
                                   oas_query_pair_t **out, size_t *out_count);
```

**Validation logic:** For each `in: query` parameter, find matching query param by name, parse value as JSON or string, validate against compiled schema.

**Tests (6):**
- `test_query_parse_simple` — `"a=1&b=2"` → 2 pairs
- `test_query_parse_empty` — `""` → 0 pairs
- `test_query_parse_encoded` — `"name=hello%20world"` → decoded value
- `test_validate_query_param_pass` — `?page=1` matches integer schema
- `test_validate_query_param_fail` — `?page=abc` fails integer schema
- `test_validate_query_required_missing` — required query param absent

**Commit:** `feat: query parameter parsing and validation (6 tests)`

---

## Task 13.4: Path Parameter Typed Extraction

**Files:**
- Modify: `src/validator/oas_request.c` — validate path params against schemas
- Modify: `src/core/oas_path_match.c` — return extracted param values
- Modify: tests

Currently path matching extracts segments as strings. Add validation: for `in: path` parameters, parse the extracted string value and validate against the parameter's schema.

**Tests (4):**
- `test_path_param_string_pass` — `/pets/fluffy` matches `{petId}` string schema
- `test_path_param_integer_pass` — `/pets/123` matches integer schema
- `test_path_param_integer_fail` — `/pets/abc` fails integer schema
- `test_path_param_enum` — `/pets/cat` matches enum `["cat", "dog"]`

**Commit:** `feat: path parameter typed validation (4 tests)`

---

## Task 13.5: RFC 9457 Problem Details Error Responses

**Files:**
- Create: `src/core/oas_problem.c`
- Create: `include/liboas/oas_problem.h`
- Modify: `CMakeLists.txt`
- Modify: tests

**API:**
```c
typedef struct {
    const char *type;       /**< URI identifying the problem type */
    const char *title;      /**< Short human-readable summary */
    int status;             /**< HTTP status code */
    const char *detail;     /**< Human-readable explanation */
    const char *instance;   /**< URI identifying the specific occurrence */
} oas_problem_t;

/**
 * @brief Create a Problem Details JSON response from validation errors.
 */
[[nodiscard]] char *oas_problem_from_validation(const oas_validation_result_t *result,
                                                 int status_code, size_t *out_len);

/**
 * @brief Create a Problem Details JSON response from a custom problem.
 */
[[nodiscard]] char *oas_problem_to_json(const oas_problem_t *problem, size_t *out_len);

/**
 * @brief Free a Problem Details JSON string.
 */
void oas_problem_free(char *json);
```

**Output format (RFC 9457):**
```json
{
  "type": "about:blank",
  "title": "Validation Failed",
  "status": 422,
  "detail": "Request body validation failed",
  "errors": [
    {"path": "/name", "message": "required property missing"},
    {"path": "/age", "message": "expected integer, got string"}
  ]
}
```

**Tests (4):**
- `test_problem_from_validation` — convert validation result to JSON
- `test_problem_custom` — custom problem type/title
- `test_problem_empty_errors` — valid result → no problem
- `test_problem_roundtrip` — parse emitted JSON, verify fields

**Commit:** `feat: RFC 9457 Problem Details error response generation (4 tests)`

---

## Task 13.6: Operation Lookup API

**Files:**
- Modify: `include/liboas/oas_adapter.h` — add `oas_adapter_find_operation()`
- Modify: `src/adapter/oas_adapter.c`
- Modify: tests

```c
typedef struct {
    const char *operation_id;
    const char *path_template;
    const char *method;
    /* Extracted path parameters */
    const char **param_names;
    const char **param_values;
    size_t param_count;
} oas_matched_operation_t;

[[nodiscard]] int oas_adapter_find_operation(const oas_adapter_t *adapter,
                                              const char *method, const char *path,
                                              oas_matched_operation_t *out);
```

**Tests (4):**
- `test_find_operation_exact` — GET /pets → listPets
- `test_find_operation_with_params` — GET /pets/123 → getPet, params: {petId: "123"}
- `test_find_operation_not_found` — GET /unknown → -ENOENT
- `test_find_operation_wrong_method` — DELETE /pets → -ENOENT (only GET/POST defined)

**Commit:** `feat: operation lookup API with path parameter extraction (4 tests)`

---

## Task 13.7: Content Negotiation

**Files:**
- Create: `src/core/oas_negotiate.c`
- Create: `include/liboas/oas_negotiate.h`
- Modify: `CMakeLists.txt`
- Modify: tests

**API:**
```c
/**
 * @brief Select best matching media type from Accept header.
 * @param accept     Accept header value (e.g., "application/json, text/html;q=0.9")
 * @param available  Array of available media types from OAS operation
 * @param count      Number of available types
 * @return Best match, or nullptr if no acceptable type
 */
[[nodiscard]] const char *oas_negotiate_content_type(const char *accept,
                                                      const char **available, size_t count);
```

Supports: quality factors (`q=0.9`), wildcards (`*/*`, `application/*`), multiple types.

**Tests (5):**
- `test_negotiate_exact_match` — `application/json` matches `application/json`
- `test_negotiate_quality_factor` — `text/html;q=0.9, application/json;q=1.0` → JSON
- `test_negotiate_wildcard` — `*/*` matches first available
- `test_negotiate_partial_wildcard` — `application/*` matches `application/json`
- `test_negotiate_no_match` — `text/xml` with only JSON available → nullptr

**Commit:** `feat: content negotiation with quality factors (5 tests)`

---

## Task 13.8: Quality Pipeline

```bash
podman run --rm --security-opt seccomp=unconfined \
  --env-file .env -v $(pwd):/workspace:Z \
  localhost/liboas-dev:latest bash -c "cd /workspace && ./scripts/quality.sh"
```

**Commit:** `chore: quality pipeline clean — Sprint 13`

---

## Summary

| Task | Feature | Tests |
|------|---------|-------|
| 13.1 | Header/query maps in request types | 3 |
| 13.2 | Header validation | 5 |
| 13.3 | Query parameter validation | 6 |
| 13.4 | Path parameter typed extraction | 4 |
| 13.5 | RFC 9457 Problem Details | 4 |
| 13.6 | Operation lookup API | 4 |
| 13.7 | Content negotiation | 5 |
| 13.8 | Quality pipeline | 0 |
| **Total** | | **31** |
