# Code-First Builder API Design

**Date:** 2026-03-09
**Status:** Approved
**Goal:** Enable code-first OpenAPI spec generation from C23 code via Builder API

## Problem

liboas currently only supports spec-first (parse existing JSON/YAML spec).
Users want code-first: define API in C code, generate OpenAPI spec automatically (like Huma in Go, FastAPI in Python, utoipa in Rust).

## Decision: Builder API + `_Generic`

Chosen over X-macros (ugly syntax, poor IDE support) and DSL codegen (extra tooling).

### Rationale

1. `oas_doc_t` / `oas_schema_t` model already exists — builder just constructs same structs
2. Emitter (Sprint 6) serializes model to JSON — completes the pipeline
3. `_Generic` provides compile-time C-type → JSON Schema type mapping
4. No external tools, IDE-friendly, debuggable
5. X-macros available as optional convenience layer on top

## Architecture

```
Builder API → oas_doc_t → compile → validate requests
                        → emit   → /openapi.json
                        → serve  → /docs (Scalar UI)
```

Both spec-first and code-first produce the same `oas_doc_t`. Everything downstream
(compiler, validator, emitter, adapter) works identically regardless of origin.

## Key Components

### Schema Builder (`src/model/oas_schema_builder.c`)

```c
/* Primitive schemas */
oas_schema_t *oas_schema_build_string(oas_arena_t *arena);
oas_schema_t *oas_schema_build_int32(oas_arena_t *arena);
oas_schema_t *oas_schema_build_int64(oas_arena_t *arena);
oas_schema_t *oas_schema_build_number(oas_arena_t *arena);
oas_schema_t *oas_schema_build_bool(oas_arena_t *arena);

/* Constrained schemas (compound literal options) */
oas_schema_t *oas_schema_build_string_ex(oas_arena_t *arena,
                                          const oas_string_opts_t *opts);

/* Composite schemas */
oas_schema_t *oas_schema_build_array(oas_arena_t *arena, oas_schema_t *items);
oas_schema_t *oas_schema_build_object(oas_arena_t *arena);
int oas_schema_add_property(oas_schema_t *schema, oas_arena_t *arena,
                            const char *name, oas_schema_t *prop);
int oas_schema_set_required(oas_schema_t *schema, oas_arena_t *arena,
                            const char *name, ...);
```

### `_Generic` Type Mapping

```c
#define oas_type_of(x) _Generic((x),          \
    int32_t:      OAS_TYPE_INTEGER,            \
    int64_t:      OAS_TYPE_INTEGER,            \
    double:       OAS_TYPE_NUMBER,             \
    float:        OAS_TYPE_NUMBER,             \
    bool:         OAS_TYPE_BOOLEAN,            \
    const char *: OAS_TYPE_STRING,             \
    char *:       OAS_TYPE_STRING,             \
    default:      OAS_TYPE_OBJECT)
```

### Document Builder (`src/model/oas_doc_builder.c`)

```c
oas_doc_t *oas_doc_build(oas_arena_t *arena, const char *title, const char *version);

int oas_doc_add_server(oas_doc_t *doc, oas_arena_t *arena,
                       const char *url, const char *description);

int oas_doc_add_path_op(oas_doc_t *doc, oas_arena_t *arena,
                        const char *path, const char *method,
                        const oas_op_builder_t *op);

int oas_doc_add_component_schema(oas_doc_t *doc, oas_arena_t *arena,
                                 const char *name, oas_schema_t *schema);
```

### Operation Builder

```c
typedef struct {
    const char *summary;
    const char *description;
    const char *operation_id;
    const char *tag;
    oas_param_builder_t *params;   /* NULL-terminated array */
    oas_schema_t *request_body;
    const char *request_content_type;  /* default: "application/json" */
    oas_response_builder_t *responses; /* NULL-terminated array */
} oas_op_builder_t;

typedef struct {
    const char *name;
    const char *in;           /* "query", "path", "header", "cookie" */
    const char *description;
    bool required;
    oas_schema_t *schema;
} oas_param_builder_t;

typedef struct {
    int status;
    const char *description;
    const char *content_type;  /* default: "application/json" */
    oas_schema_t *schema;
} oas_response_builder_t;
```

### Usage Example

```c
oas_arena_t *arena = oas_arena_create(0);
oas_doc_t *doc = oas_doc_build(arena, "Pet Store", "1.0.0");

/* Schema */
oas_schema_t *pet = oas_schema_build_object(arena);
oas_schema_add_property(pet, arena, "id", oas_schema_build_int64(arena));
oas_schema_add_property(pet, arena, "name", oas_schema_build_string(arena));
oas_schema_set_required(pet, arena, "id", "name", NULL);
oas_doc_add_component_schema(doc, arena, "Pet", pet);

/* Operation */
oas_doc_add_path_op(doc, arena, "/pets", "get", &(oas_op_builder_t){
    .summary = "List pets",
    .operation_id = "listPets",
    .params = (oas_param_builder_t[]){
        {.name = "limit", .in = "query",
         .schema = oas_schema_build_int32(arena)},
        {0}
    },
    .responses = (oas_response_builder_t[]){
        {.status = 200, .description = "Pet list",
         .schema = oas_schema_build_array(arena, pet)},
        {0}
    },
});

/* Generate spec */
size_t len;
char *json = oas_doc_emit_json(doc, nullptr, &len);
/* json contains complete OpenAPI 3.2 spec */

/* Or compile for validation */
oas_compiled_doc_t *compiled = oas_doc_compile(doc, &config, &errors);
```

### Adapter Integration

```c
/* Spec-first (existing) */
oas_adapter_t *a = oas_adapter_create(&(oas_adapter_config_t){
    .spec_path = "petstore.json"
});

/* Code-first (new) */
oas_adapter_t *a = oas_adapter_from_doc(doc, &(oas_adapter_config_t){
    .serve_spec = true,
    .serve_scalar = true,
});
```

## Sprint Placement

New task **6.3: Schema & Document Builder API** after emitter (6.1):
- Depends on: model types (Sprint 2, done), emitter (6.1)
- Files: `include/liboas/oas_builder.h`, `src/model/oas_schema_builder.c`,
  `src/model/oas_doc_builder.c`, `tests/unit/test_builder.c`
- Tests: ~16 (schema primitives, constraints, objects, arrays, document,
  operations, params, responses, component refs, `_Generic` mapping,
  roundtrip with emitter)

## Research References

- Go/Huma: runtime reflection + struct tags
- Rust/utoipa: compile-time proc macros + derive
- Python/FastAPI: type hints + Pydantic introspection
- Java/SpringDoc: runtime reflection + annotations
- C#/.NET: runtime reflection + attributes
- C has none of these — Builder API is the pragmatic choice
