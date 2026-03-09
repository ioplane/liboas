# API Reference

All public functions use the `oas_` prefix. Functions return 0 on success or
negative errno on failure unless otherwise noted. Pointers marked `[[nodiscard]]`
must not be ignored.

## oas_alloc -- Arena Allocator

```c
oas_arena_t *oas_arena_create(size_t block_size);
```
Create a new arena. Pass 0 for the default 64 KiB block size. Returns `nullptr`
on allocation failure.

```c
void oas_arena_destroy(oas_arena_t *arena);
```
Destroy arena and free all blocks. Nullptr-safe.

```c
void *oas_arena_alloc(oas_arena_t *arena, size_t size, size_t align);
```
Allocate `size` bytes with `align` alignment. Returns `nullptr` on failure or
zero size.

```c
void oas_arena_reset(oas_arena_t *arena);
```
Reset arena for reuse without freeing blocks. All prior allocations are invalid.

```c
size_t oas_arena_used(const oas_arena_t *arena);
```
Return total bytes allocated across all blocks.

## oas_error -- Error Accumulation

```c
oas_error_list_t *oas_error_list_create(oas_arena_t *arena);
```
Create an error list backed by an arena. Returns `nullptr` on failure.

```c
void oas_error_list_add(oas_error_list_t *list, oas_error_kind_t kind,
                        const char *path, const char *fmt, ...);
```
Add an error with printf-style formatting. Path and message are copied into
the arena.

```c
size_t oas_error_list_count(const oas_error_list_t *list);
```
Return the number of errors.

```c
const oas_error_t *oas_error_list_get(const oas_error_list_t *list, size_t index);
```
Get error at index. Returns `nullptr` if out of range.

```c
bool oas_error_list_has_errors(const oas_error_list_t *list);
```
Return true if the list contains any errors.

```c
const char *oas_error_kind_name(oas_error_kind_t kind);
```
Return human-readable name for an error kind (e.g. `"TYPE"`, `"CONSTRAINT"`).

## oas_parser -- Document Parsing

```c
oas_doc_t *oas_doc_parse(oas_arena_t *arena, const char *json, size_t len,
                         oas_error_list_t *errors);
```
Parse an OpenAPI 3.2 document from a JSON string. Returns `nullptr` on failure.

```c
oas_doc_t *oas_doc_parse_file(oas_arena_t *arena, const char *path,
                              oas_error_list_t *errors);
```
Parse an OpenAPI 3.2 document from a file path. Returns `nullptr` on failure.

```c
void oas_doc_free(oas_doc_t *doc);
```
Free the underlying yyjson document. Invalidates all zero-copy string pointers.

## oas_schema -- JSON Schema Model

```c
oas_schema_t *oas_schema_create(oas_arena_t *arena);
```
Allocate and zero-initialize a schema with sentinel defaults.

```c
int oas_schema_add_property(oas_arena_t *arena, oas_schema_t *schema,
                            const char *name, oas_schema_t *prop_schema);
```
Append a property to an object schema's property list.

```c
uint8_t oas_type_from_string(const char *type_name);
```
Convert type name to bitmask (e.g. `"string"` -> `OAS_TYPE_STRING`). Returns 0
for unknown types.

## oas_compiler -- Schema Compilation

```c
oas_compiled_schema_t *oas_schema_compile(const oas_schema_t *schema,
                                          const oas_compiler_config_t *config,
                                          oas_error_list_t *errors);
```
Compile a schema tree into validation bytecode. Returns `nullptr` on failure.

```c
void oas_compiled_schema_free(oas_compiled_schema_t *compiled);
```
Free a compiled schema and all its resources (instructions, regex patterns).

```c
oas_compiled_doc_t *oas_doc_compile(const oas_doc_t *doc,
                                    const oas_compiler_config_t *config,
                                    oas_error_list_t *errors);
```
Compile all schemas in a parsed document. Returns `nullptr` on failure.

```c
void oas_compiled_doc_free(oas_compiled_doc_t *compiled);
```
Free a compiled document and all compiled schemas.

## oas_validator -- Validation Engine

```c
int oas_validate(const oas_compiled_schema_t *compiled, yyjson_val *value,
                 oas_validation_result_t *result, oas_arena_t *arena);
```
Validate a yyjson value against a compiled schema. Populates `result`.

```c
int oas_validate_json(const oas_compiled_schema_t *compiled, const char *json,
                      size_t len, oas_validation_result_t *result,
                      oas_arena_t *arena);
```
Parse JSON string and validate against a compiled schema.

```c
int oas_validate_request(const oas_compiled_doc_t *doc,
                         const oas_http_request_t *req,
                         oas_validation_result_t *result, oas_arena_t *arena);
```
Validate an HTTP request (path, parameters, body) against a compiled document.

```c
int oas_validate_response(const oas_compiled_doc_t *doc, const char *path,
                          const char *method, const oas_http_response_t *resp,
                          oas_validation_result_t *result, oas_arena_t *arena);
```
Validate an HTTP response against a compiled document. Requires the original
request path and method.

## oas_emitter -- JSON Emission

```c
char *oas_doc_emit_json(const oas_doc_t *doc, const oas_emit_options_t *options,
                        size_t *out_len);
```
Emit a document as a JSON string. Returns heap-allocated string. Free with
`oas_emit_free()`.

```c
char *oas_schema_emit_json(const oas_schema_t *schema,
                           const oas_emit_options_t *options, size_t *out_len);
```
Emit a schema as a JSON string. Free with `oas_emit_free()`.

```c
void oas_emit_free(char *json);
```
Free a string returned by emission functions. Nullptr-safe.

## oas_builder -- Code-First Construction

```c
oas_schema_t *oas_schema_build_string(oas_arena_t *arena);
oas_schema_t *oas_schema_build_int32(oas_arena_t *arena);
oas_schema_t *oas_schema_build_int64(oas_arena_t *arena);
oas_schema_t *oas_schema_build_number(oas_arena_t *arena);
oas_schema_t *oas_schema_build_bool(oas_arena_t *arena);
oas_schema_t *oas_schema_build_string_ex(oas_arena_t *arena, const oas_string_opts_t *opts);
oas_schema_t *oas_schema_build_integer_ex(oas_arena_t *arena, const oas_number_opts_t *opts);
oas_schema_t *oas_schema_build_number_ex(oas_arena_t *arena, const oas_number_opts_t *opts);
oas_schema_t *oas_schema_build_array(oas_arena_t *arena, oas_schema_t *items);
oas_schema_t *oas_schema_build_object(oas_arena_t *arena);
```
Schema factory functions. Each returns a schema with the appropriate `type_mask`.

```c
int oas_schema_set_required(oas_arena_t *arena, oas_schema_t *schema, ...);
```
Set required property names. Pass a NULL-terminated list of strings.

```c
int oas_schema_set_description(oas_schema_t *schema, const char *description);
int oas_schema_set_additional_properties(oas_schema_t *schema, oas_schema_t *additional);
```
Set schema metadata and constraints.

```c
oas_doc_t *oas_doc_build(oas_arena_t *arena, const char *title, const char *version);
```
Create a minimal OAS 3.2.0 document with `info.title` and `info.version`.

```c
int oas_doc_add_server(oas_doc_t *doc, oas_arena_t *arena, const char *url,
                       const char *description);
int oas_doc_add_component_schema(oas_doc_t *doc, oas_arena_t *arena,
                                 const char *name, oas_schema_t *schema);
int oas_doc_add_path_op(oas_doc_t *doc, oas_arena_t *arena, const char *path,
                        const char *method, const oas_op_builder_t *op);
```
Add servers, component schemas, and path operations to a document.

## oas_adapter -- Integration Facade

```c
oas_adapter_t *oas_adapter_create(const char *json, size_t len,
                                  const oas_adapter_config_t *config,
                                  oas_error_list_t *errors);
```
Create adapter from JSON spec string (spec-first).

```c
oas_adapter_t *oas_adapter_from_doc(const oas_doc_t *doc, oas_arena_t *arena,
                                    const oas_adapter_config_t *config,
                                    oas_error_list_t *errors);
```
Create adapter from builder-constructed document (code-first).

```c
void oas_adapter_destroy(oas_adapter_t *adapter);
```
Destroy adapter and free all resources.

```c
const oas_doc_t *oas_adapter_doc(const oas_adapter_t *adapter);
const oas_adapter_config_t *oas_adapter_config(const oas_adapter_t *adapter);
```
Access the parsed document and configuration.

```c
const char *oas_adapter_spec_json(const oas_adapter_t *adapter, size_t *out_len);
```
Get cached spec JSON for serving. Returns `nullptr` if `serve_spec` is disabled.

```c
int oas_adapter_validate_request(const oas_adapter_t *adapter,
                                 const oas_http_request_t *req,
                                 oas_validation_result_t *result, oas_arena_t *arena);
int oas_adapter_validate_response(const oas_adapter_t *adapter, const char *path,
                                  const char *method, const oas_http_response_t *resp,
                                  oas_validation_result_t *result, oas_arena_t *arena);
```
Validate requests and responses via the adapter.

```c
int oas_adapter_find_operation(const oas_adapter_t *adapter, const char *method,
                               const char *path, oas_matched_operation_t *out,
                               oas_arena_t *arena);
```
Look up an operation by method and path. Returns `-ENOENT` if not found.

```c
char *oas_scalar_html(const char *title, const char *spec_url, size_t *out_len);
```
Generate Scalar API documentation HTML. Caller must `free()` the result.

## oas_regex -- Regex Backend

```c
oas_regex_backend_t *oas_regex_libregexp_create(void);
```
Create the default regex backend using vendored QuickJS libregexp. Returns
`nullptr` on failure.

## oas_problem -- RFC 9457 Problem Details

```c
char *oas_problem_from_validation(const oas_validation_result_t *result,
                                  int status_code, size_t *out_len);
```
Create Problem Details JSON from validation errors. Returns `nullptr` if valid.
Free with `oas_problem_free()`.

```c
char *oas_problem_to_json(const oas_problem_t *problem, size_t *out_len);
```
Create Problem Details JSON from a custom problem struct. Free with
`oas_problem_free()`.

```c
void oas_problem_free(char *json);
```
Free a Problem Details JSON string.

## oas_negotiate -- Content Negotiation

```c
const char *oas_negotiate_content_type(const char *accept, const char **available,
                                       size_t count);
```
Select best matching media type from an Accept header. Returns `nullptr` if
no acceptable type.
