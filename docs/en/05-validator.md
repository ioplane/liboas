# Validation Engine

The validation engine executes compiled schema bytecode against JSON values and
HTTP messages. Header: `oas_validator.h`.

## Schema Validation

### Validate a yyjson Value

```c
oas_validation_result_t result = {0};
int rc = oas_validate(compiled_schema, yyjson_value, &result, arena);
if (rc < 0) {
    /* invalid arguments (-EINVAL) */
}
if (!result.valid) {
    /* inspect result.errors */
}
```

`oas_validate()` takes a pre-compiled schema and a `yyjson_val *`. It
populates the `oas_validation_result_t` with a `valid` flag and an error list.

### Validate a JSON String

```c
oas_validation_result_t result = {0};
int rc = oas_validate_json(compiled_schema, json_str, json_len, &result, arena);
```

`oas_validate_json()` parses the JSON string first, then validates the
resulting value. Parse failures are reported as `OAS_ERR_PARSE` errors.

## Validation Result

```c
typedef struct {
    bool valid;                /* true if all checks passed */
    oas_error_list_t *errors;  /* accumulated errors (arena-allocated) */
} oas_validation_result_t;
```

The result is valid when `valid` is true and the error list is empty.
When validation fails, all constraint violations are collected -- the validator
does not stop at the first error.

## HTTP Request Validation

```c
oas_http_request_t req = {
    .method       = "POST",
    .path         = "/pets",
    .content_type = "application/json",
    .body         = body_json,
    .body_len     = body_len,
    .headers      = headers,
    .headers_count = header_count,
    .query        = query_params,
    .query_count  = query_count,
};

oas_validation_result_t result = {0};
int rc = oas_validate_request(compiled_doc, &req, &result, arena);
```

Request validation performs these steps:

1. **Path matching** -- match `req.path` against all path templates in the
   compiled document, extracting path parameters.
2. **Method lookup** -- find the operation for the matched path and HTTP method.
3. **Parameter validation** -- validate path, query, header, and cookie
   parameters against their declared schemas.
4. **Request body validation** -- if the operation declares a request body,
   validate `req.body` against the schema for the matching content type.

### oas_http_request_t

| Field          | Type                        | Description                    |
|----------------|-----------------------------|--------------------------------|
| `method`       | `const char *`              | HTTP method (`"GET"`, `"POST"`) |
| `path`         | `const char *`              | Request path (e.g. `"/pets/123"`) |
| `content_type` | `const char *`              | Content-Type header (nullable) |
| `body`         | `const char *`              | Request body (nullable)        |
| `body_len`     | `size_t`                    | Body length in bytes           |
| `headers`      | `const oas_http_header_t *` | Header name/value pairs        |
| `headers_count`| `size_t`                    | Number of headers              |
| `query`        | `const oas_http_query_param_t *` | Query param pairs         |
| `query_count`  | `size_t`                    | Number of query parameters     |
| `query_string` | `const char *`              | Raw query string (nullable)    |

## HTTP Response Validation

```c
oas_http_response_t resp = {
    .status_code  = 200,
    .content_type = "application/json",
    .body         = response_json,
    .body_len     = response_len,
    .headers      = resp_headers,
    .headers_count = resp_header_count,
};

oas_validation_result_t result = {0};
int rc = oas_validate_response(compiled_doc, "/pets", "GET", &resp, &result, arena);
```

Response validation requires the original request path and method to locate
the correct operation and response definition.

### oas_http_response_t

| Field          | Type                        | Description                    |
|----------------|-----------------------------|--------------------------------|
| `status_code`  | `int`                       | HTTP status code (e.g. 200)    |
| `content_type` | `const char *`              | Content-Type header (nullable) |
| `body`         | `const char *`              | Response body (nullable)       |
| `body_len`     | `size_t`                    | Body length in bytes           |
| `headers`      | `const oas_http_header_t *` | Header name/value pairs        |
| `headers_count`| `size_t`                    | Number of headers              |

## Path Matching

The validator matches request paths against OpenAPI path templates using a
segment-by-segment comparison:

- Literal segments must match exactly.
- Templated segments (`{petId}`) match any non-empty value.
- Path parameters are extracted and validated against their schemas.

Example: path template `/pets/{petId}` matches `/pets/123` and extracts
`petId = "123"`.

## Status Code Matching

Response lookup follows a priority chain:

1. **Exact match** -- e.g. status `200` matches response key `"200"`.
2. **Range match** -- e.g. status `201` matches `"2XX"`.
3. **Default** -- the `"default"` response matches any unmatched status.

If no response definition matches, validation reports an error.

## Error Accumulation

Validation collects all errors rather than failing on the first violation.
Errors are stored in `oas_error_list_t` (arena-allocated).

### Error Structure

```c
typedef struct {
    oas_error_kind_t kind;    /* error category */
    const char *message;      /* human-readable description */
    const char *path;         /* JSON Pointer to error location */
    uint32_t line;            /* source line (parse errors) */
    uint32_t column;          /* source column (parse errors) */
} oas_error_t;
```

### Error Kinds

| Kind               | Description                       |
|--------------------|-----------------------------------|
| `OAS_ERR_PARSE`    | JSON/YAML parse error             |
| `OAS_ERR_SCHEMA`   | JSON Schema structural error      |
| `OAS_ERR_REF`      | `$ref` resolution failure         |
| `OAS_ERR_TYPE`     | Type mismatch                     |
| `OAS_ERR_CONSTRAINT` | min/max/pattern violation       |
| `OAS_ERR_REQUIRED` | Missing required field            |
| `OAS_ERR_FORMAT`   | Format validation failure         |
| `OAS_ERR_ALLOC`    | Memory allocation failure         |

### JSON Pointer Paths

Each error includes a JSON Pointer (RFC 6901) path indicating the location
of the error in the input document. For nested objects, paths look like
`/address/zipCode`. For arrays, paths use integer indices: `/items/2/name`.

### Iterating Errors

```c
size_t count = oas_error_list_count(result.errors);
for (size_t i = 0; i < count; i++) {
    const oas_error_t *err = oas_error_list_get(result.errors, i);
    fprintf(stderr, "[%s] %s at %s\n",
            oas_error_kind_name(err->kind), err->message, err->path);
}
```

## RFC 9457 Problem Details

Validation errors can be converted to RFC 9457 Problem Details JSON using
`oas_problem.h`:

```c
char *json = oas_problem_from_validation(&result, 422, &json_len);
/* send json as HTTP response body */
oas_problem_free(json);
```

This produces a structured error response suitable for returning to API
clients.
