# liboas Sprint Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task.

**Goal:** Build a C23 library for parsing, validating, and serving OpenAPI 3.2 specifications with zero-allocation runtime validation and iohttp integration.

**Architecture:** Two-layer design — OAS Model (parse OpenAPI JSON/YAML into in-memory document model with $ref resolution) and Compiled Runtime (pre-compile JSON Schema 2020-12 to flat instruction arrays for zero-allocation request/response validation). Arena allocator for document lifetime. Adapter pattern for iohttp integration.

**Tech Stack:** C23, yyjson 0.12+ (JSON), PCRE2 (regex default), QuickJS libregexp (ECMA-262 strict, vendored), libfyaml (optional YAML 1.2), Unity tests, Linux kernel 6.7+.

**Build/test:**
```bash
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
```

**Reference docs:**
- `docs/tmp/draft/developing-an-openapi-library-on-modern-c23.md` — research
- `docs/tmp/draft/design-and-development-of-the-liboas-library.md` — architecture design
- `.claude/skills/liboas-architecture/SKILL.md` — architecture reference
- `.claude/skills/json-schema-patterns/SKILL.md` — JSON Schema patterns
- `.claude/skills/rfc-reference/SKILL.md` — RFC reference

---

## Sprint 1: Core Infrastructure (2-3 weeks)

**Goal:** Arena allocator, error handling, string interning, yyjson integration — the foundation for everything.

### Task 1.1: Arena Allocator

**Files:**
- Create: `include/liboas/oas_alloc.h`
- Create: `src/core/oas_alloc.c`
- Create: `tests/unit/test_alloc.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Arena allocator for document model lifetime — all nodes allocated from single arena, freed together in O(1).

```c
constexpr size_t OAS_ARENA_DEFAULT_BLOCK = 64 * 1024;  /* 64 KiB blocks */

typedef struct oas_arena oas_arena_t;

[[nodiscard]] oas_arena_t *oas_arena_create(size_t block_size);
void oas_arena_destroy(oas_arena_t *arena);
[[nodiscard]] void *oas_arena_alloc(oas_arena_t *arena, size_t size, size_t align);
void oas_arena_reset(oas_arena_t *arena);  /* reuse without free */
size_t oas_arena_used(const oas_arena_t *arena);
```

Internal struct:
```c
typedef struct oas_arena_block {
    struct oas_arena_block *next;
    size_t capacity;
    size_t used;
    alignas(max_align_t) uint8_t data[];
} oas_arena_block_t;

struct oas_arena {
    oas_arena_block_t *current;
    oas_arena_block_t *head;
    size_t block_size;
    size_t total_allocated;
};
```

**Tests (8):**
```c
void test_arena_create_destroy(void);
void test_arena_alloc_returns_aligned(void);
void test_arena_alloc_multiple(void);
void test_arena_alloc_large_object(void);     /* > block_size triggers new block */
void test_arena_reset(void);                   /* used → 0, blocks reused */
void test_arena_used_tracking(void);
void test_arena_null_on_zero_size(void);
void test_arena_alignment_power_of_two(void);
```

**CMakeLists.txt addition:**
```cmake
add_library(oas_core STATIC src/core/oas_alloc.c)
target_include_directories(oas_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(oas_core PUBLIC _GNU_SOURCE)

oas_add_test(test_alloc tests/unit/test_alloc.c oas_core)
```

**Commit:** `feat: arena allocator for document model lifetime (8 tests)`

---

### Task 1.2: Error Handling

**Files:**
- Create: `include/liboas/oas_error.h`
- Create: `src/core/oas_error.c`
- Create: `tests/unit/test_error.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Error accumulation — validator collects all errors, not just first.

```c
typedef enum : uint8_t {
    OAS_ERR_NONE = 0,
    OAS_ERR_PARSE,          /* JSON/YAML parse error */
    OAS_ERR_SCHEMA,         /* JSON Schema validation error */
    OAS_ERR_REF,            /* $ref resolution error */
    OAS_ERR_TYPE,           /* type mismatch */
    OAS_ERR_CONSTRAINT,     /* min/max/pattern violation */
    OAS_ERR_REQUIRED,       /* missing required field */
    OAS_ERR_FORMAT,         /* format validation failure */
    OAS_ERR_ALLOC,          /* memory allocation failure */
} oas_error_kind_t;

typedef struct {
    oas_error_kind_t kind;
    const char *message;       /* arena-allocated */
    const char *path;          /* JSON Pointer to error location */
    uint32_t line;             /* source line (for parse errors) */
    uint32_t column;           /* source column */
} oas_error_t;

typedef struct oas_error_list oas_error_list_t;

[[nodiscard]] oas_error_list_t *oas_error_list_create(oas_arena_t *arena);
void oas_error_list_add(oas_error_list_t *list, oas_error_kind_t kind,
                        const char *path, const char *fmt, ...);
size_t oas_error_list_count(const oas_error_list_t *list);
const oas_error_t *oas_error_list_get(const oas_error_list_t *list, size_t index);
bool oas_error_list_has_errors(const oas_error_list_t *list);
const char *oas_error_kind_name(oas_error_kind_t kind);
```

**Tests (8):**
```c
void test_error_list_create(void);
void test_error_list_add_single(void);
void test_error_list_add_multiple(void);
void test_error_list_count(void);
void test_error_list_get_by_index(void);
void test_error_list_has_errors(void);
void test_error_list_empty_has_no_errors(void);
void test_error_kind_name(void);
```

**Commit:** `feat: error accumulation with JSON Pointer paths (8 tests)`

---

### Task 1.3: String Interning

**Files:**
- Create: `src/core/oas_intern.h`
- Create: `src/core/oas_intern.c`
- Create: `tests/unit/test_intern.c`
- Modify: `CMakeLists.txt`

**Implementation:**

String interning for repeated keys (type, properties, required, etc.). Uses arena + hash table.

```c
typedef struct oas_intern oas_intern_t;

[[nodiscard]] oas_intern_t *oas_intern_create(oas_arena_t *arena, size_t capacity);
const char *oas_intern_get(oas_intern_t *pool, const char *str, size_t len);
size_t oas_intern_count(const oas_intern_t *pool);
```

Same string always returns same pointer → enables pointer comparison instead of strcmp.

**Tests (6):**
```c
void test_intern_create(void);
void test_intern_same_string_same_pointer(void);
void test_intern_different_strings(void);
void test_intern_empty_string(void);
void test_intern_count(void);
void test_intern_with_embedded_null(void);  /* uses len parameter */
```

**Commit:** `feat: string interning pool for repeated schema keys (6 tests)`

---

### Task 1.4: yyjson Integration Wrapper

**Files:**
- Create: `src/parser/oas_json.h`
- Create: `src/parser/oas_json.c`
- Create: `tests/unit/test_json.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Thin wrapper around yyjson for consistent error handling and arena integration.

```c
typedef struct {
    yyjson_doc *doc;     /* yyjson document (owns memory) */
    yyjson_val *root;    /* root value */
} oas_json_doc_t;

[[nodiscard]] int oas_json_parse(const char *data, size_t len, oas_json_doc_t *out,
                                  oas_error_list_t *errors);
[[nodiscard]] int oas_json_parse_file(const char *path, oas_json_doc_t *out,
                                       oas_error_list_t *errors);
void oas_json_free(oas_json_doc_t *doc);

/* Helpers for typed access with error accumulation */
[[nodiscard]] const char *oas_json_get_str(yyjson_val *obj, const char *key);
[[nodiscard]] int64_t oas_json_get_int(yyjson_val *obj, const char *key, int64_t def);
[[nodiscard]] bool oas_json_get_bool(yyjson_val *obj, const char *key, bool def);
[[nodiscard]] yyjson_val *oas_json_get_obj(yyjson_val *obj, const char *key);
[[nodiscard]] yyjson_val *oas_json_get_arr(yyjson_val *obj, const char *key);
```

**Tests (8):**
```c
void test_json_parse_valid(void);
void test_json_parse_invalid(void);
void test_json_parse_empty(void);
void test_json_get_str(void);
void test_json_get_int_default(void);
void test_json_get_bool_default(void);
void test_json_get_obj_missing(void);
void test_json_parse_file_not_found(void);
```

**Commit:** `feat: yyjson integration wrapper with typed accessors (8 tests)`

---

## Sprint 2: OAS Document Model (3-4 weeks)

**Goal:** Parse OpenAPI 3.2 JSON into typed document model with all core objects.

### Task 2.1: Schema Type Representation

**Files:**
- Create: `include/liboas/oas_schema.h`
- Create: `src/model/oas_schema.c`
- Create: `tests/unit/test_schema.c`
- Modify: `CMakeLists.txt`

**Implementation:**

JSON Schema 2020-12 node representation — the core data structure.

```c
typedef enum : uint8_t {
    OAS_TYPE_NULL    = 0x01,
    OAS_TYPE_BOOLEAN = 0x02,
    OAS_TYPE_INTEGER = 0x04,
    OAS_TYPE_NUMBER  = 0x08,
    OAS_TYPE_STRING  = 0x10,
    OAS_TYPE_ARRAY   = 0x20,
    OAS_TYPE_OBJECT  = 0x40,
} oas_type_t;

typedef struct oas_schema oas_schema_t;

struct oas_schema {
    uint8_t type_mask;              /* bitmask of OAS_TYPE_* (supports type arrays) */
    const char *title;
    const char *description;
    const char *format;             /* date, date-time, email, uri, uuid, etc. */

    /* String constraints */
    int64_t min_length, max_length; /* -1 = not set */
    const char *pattern;            /* ECMA-262 regex (backend-agnostic) */

    /* Numeric constraints */
    double minimum, maximum;
    double exclusive_minimum, exclusive_maximum;
    double multiple_of;
    bool has_minimum, has_maximum, has_exclusive_minimum, has_exclusive_maximum, has_multiple_of;

    /* Array constraints */
    oas_schema_t *items;            /* items schema */
    oas_schema_t **prefix_items;    /* tuple validation (2020-12) */
    size_t prefix_items_count;
    int64_t min_items, max_items;   /* -1 = not set */
    bool unique_items;

    /* Object constraints */
    struct oas_property *properties;     /* linked list */
    size_t properties_count;
    const char **required;               /* array of required property names */
    size_t required_count;
    oas_schema_t *additional_properties; /* nullptr = no constraint, bool/schema */
    bool additional_properties_bool;     /* true if additionalProperties is boolean */

    /* Composition */
    oas_schema_t **all_of;
    size_t all_of_count;
    oas_schema_t **any_of;
    size_t any_of_count;
    oas_schema_t **one_of;
    size_t one_of_count;
    oas_schema_t *not_schema;

    /* Conditional */
    oas_schema_t *if_schema;
    oas_schema_t *then_schema;
    oas_schema_t *else_schema;

    /* Const/Enum */
    yyjson_val *const_value;        /* zero-copy from yyjson doc */
    yyjson_val *enum_values;        /* array of allowed values */

    /* $ref */
    const char *ref;                /* raw $ref string */
    oas_schema_t *ref_resolved;     /* resolved target (nullptr if unresolved) */

    /* Default */
    yyjson_val *default_value;

    /* Nullable (OAS 3.0 compat: nullable → type includes null) */
    bool nullable;

    /* Read/Write only */
    bool read_only;
    bool write_only;

    /* Discriminator (OpenAPI 3.2 polymorphism) */
    const char *discriminator_property;         /* propertyName */
    struct oas_discriminator_mapping *discriminator_mapping; /* mapping entries */
    size_t discriminator_mapping_count;

    /* 2020-12 advanced */
    bool has_unevaluated_properties;
    oas_schema_t *unevaluated_properties;
    bool has_unevaluated_items;
    oas_schema_t *unevaluated_items;
};

typedef struct oas_discriminator_mapping {
    const char *key;           /* mapping key */
    const char *ref;           /* $ref to schema */
} oas_discriminator_mapping_t;

typedef struct oas_property {
    const char *name;
    oas_schema_t *schema;
    struct oas_property *next;
} oas_property_t;
```

**Tests (10):**
```c
void test_schema_type_mask_single(void);
void test_schema_type_mask_array(void);       /* ["string", "null"] */
void test_schema_string_constraints(void);
void test_schema_numeric_constraints(void);
void test_schema_array_items(void);
void test_schema_object_properties(void);
void test_schema_required_fields(void);
void test_schema_composition_allof(void);
void test_schema_ref_string(void);
void test_schema_nullable_to_type_mask(void); /* nullable: true → add OAS_TYPE_NULL */
void test_schema_discriminator(void);        /* discriminator property + mapping */
void test_schema_unevaluated_properties(void);
```

**Commit:** `feat: JSON Schema 2020-12 type representation (12 tests)`

---

### Task 2.2: Schema Parser (JSON → oas_schema_t)

**Files:**
- Create: `src/parser/oas_schema_parser.h`
- Create: `src/parser/oas_schema_parser.c`
- Create: `tests/unit/test_schema_parser.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Parse yyjson value into `oas_schema_t` tree using arena allocator.

```c
[[nodiscard]] oas_schema_t *oas_schema_parse(oas_arena_t *arena, yyjson_val *val,
                                              oas_error_list_t *errors);
```

Recursively walks JSON, handles all keywords from Task 2.1.

**Tests (12):**
```c
void test_parse_simple_string(void);         /* {"type": "string"} */
void test_parse_string_constraints(void);    /* minLength, maxLength, pattern */
void test_parse_integer_range(void);         /* minimum, maximum, exclusiveMinimum */
void test_parse_array_items(void);           /* {"type": "array", "items": {...}} */
void test_parse_tuple_prefix_items(void);    /* prefixItems (2020-12) */
void test_parse_object_properties(void);     /* properties + required */
void test_parse_allof(void);                 /* allOf composition */
void test_parse_oneof(void);                 /* oneOf composition */
void test_parse_anyof(void);                 /* anyOf composition */
void test_parse_if_then_else(void);          /* conditional */
void test_parse_ref(void);                   /* $ref string stored */
void test_parse_nullable(void);              /* nullable → type_mask |= NULL */
```

**Commit:** `feat: JSON Schema parser — JSON to oas_schema_t tree (12 tests)`

---

### Task 2.3: OAS Document Model Types

**Files:**
- Create: `include/liboas/oas_doc.h`
- Create: `src/model/oas_doc.c`
- Create: `tests/unit/test_doc.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Top-level OpenAPI document model.

```c
typedef struct {
    const char *openapi;       /* "3.2.0" */
    oas_info_t *info;
    oas_server_t **servers;
    size_t servers_count;
    oas_path_map_t *paths;     /* hash map: path → path_item */
    oas_components_t *components;
    oas_security_req_t **security;
    size_t security_count;
    oas_tag_t **tags;
    size_t tags_count;
} oas_doc_t;

typedef struct {
    const char *title;
    const char *description;
    const char *version;
    oas_contact_t *contact;
    oas_license_t *license;
} oas_info_t;

typedef struct {
    const char *url;
    const char *description;
    oas_server_var_t **variables;
    size_t variables_count;
} oas_server_t;

typedef struct {
    oas_operation_t *get;
    oas_operation_t *post;
    oas_operation_t *put;
    oas_operation_t *delete_;
    oas_operation_t *patch;
    oas_operation_t *head;
    oas_operation_t *options;
    oas_parameter_t **parameters;
    size_t parameters_count;
} oas_path_item_t;

typedef struct {
    const char *operation_id;
    const char *summary;
    const char *description;
    const char **tags;
    size_t tags_count;
    oas_parameter_t **parameters;
    size_t parameters_count;
    oas_request_body_t *request_body;
    oas_response_map_t *responses;
    oas_security_req_t **security;
    size_t security_count;
    bool deprecated;
} oas_operation_t;
```

**Tests (10):**
```c
void test_doc_types_info(void);
void test_doc_types_server(void);
void test_doc_types_path_item(void);
void test_doc_types_operation(void);
void test_doc_types_parameter(void);
void test_doc_types_request_body(void);
void test_doc_types_response(void);
void test_doc_types_media_type(void);
void test_doc_types_components(void);
void test_doc_types_security_scheme(void);
```

**Commit:** `feat: OAS document model types — paths, operations, schemas (10 tests)`

---

### Task 2.4: Document Parser

**Files:**
- Create: `include/liboas/oas_parser.h`
- Create: `src/parser/oas_doc_parser.c`
- Create: `tests/unit/test_doc_parser.c`
- Create: `tests/fixtures/petstore.json` (minimal Petstore spec)
- Modify: `CMakeLists.txt`

**Implementation:**

Parse full OpenAPI 3.2 JSON document into `oas_doc_t`.

```c
[[nodiscard]] oas_doc_t *oas_doc_parse(oas_arena_t *arena, const char *json, size_t len,
                                        oas_error_list_t *errors);
[[nodiscard]] oas_doc_t *oas_doc_parse_file(oas_arena_t *arena, const char *path,
                                             oas_error_list_t *errors);
void oas_doc_free(oas_doc_t *doc);  /* frees arena */
```

**Tests (10):**
```c
void test_doc_parse_minimal(void);           /* minimal valid OAS 3.2 */
void test_doc_parse_petstore(void);          /* fixture file */
void test_doc_parse_invalid_json(void);
void test_doc_parse_missing_openapi(void);   /* no "openapi" field */
void test_doc_parse_wrong_version(void);     /* "2.0" → error */
void test_doc_parse_info_fields(void);
void test_doc_parse_servers(void);
void test_doc_parse_paths(void);
void test_doc_parse_operations(void);
void test_doc_parse_components_schemas(void);
```

**Commit:** `feat: OpenAPI 3.2 document parser with error accumulation (10 tests)`

---

### Task 2.5: YAML Parsing (libfyaml, optional)

**Files:**
- Create: `src/parser/oas_yaml.h`
- Create: `src/parser/oas_yaml.c`
- Create: `tests/unit/test_yaml.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Optional YAML 1.2 parsing via libfyaml. Converts YAML to yyjson document for uniform processing.

```c
#ifdef OAS_HAVE_LIBFYAML
[[nodiscard]] int oas_yaml_parse(const char *data, size_t len, oas_json_doc_t *out,
                                  oas_error_list_t *errors);
[[nodiscard]] int oas_yaml_parse_file(const char *path, oas_json_doc_t *out,
                                       oas_error_list_t *errors);
#endif

/* Auto-detect format (JSON or YAML) by content inspection */
[[nodiscard]] int oas_parse_auto(const char *data, size_t len, oas_json_doc_t *out,
                                  oas_error_list_t *errors);
```

Strategy: libfyaml parses YAML to its DOM, then we walk it and build a yyjson mutable document.
This keeps all downstream code (schema parser, doc parser) JSON-only.

CMake:
```cmake
option(OAS_YAML "Enable YAML 1.2 parsing via libfyaml" OFF)
if(OAS_YAML)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBFYAML REQUIRED libfyaml)
    target_compile_definitions(oas_core PUBLIC OAS_HAVE_LIBFYAML)
endif()
```

**Tests (8):**
```c
void test_yaml_parse_simple(void);           /* key: value */
void test_yaml_parse_nested(void);           /* nested objects/arrays */
void test_yaml_parse_multiline_string(void); /* literal/folded blocks */
void test_yaml_parse_anchors_aliases(void);  /* YAML &anchor/*alias */
void test_yaml_parse_invalid(void);          /* syntax error */
void test_yaml_parse_file(void);
void test_auto_detect_json(void);            /* starts with { */
void test_auto_detect_yaml(void);            /* starts with openapi: */
```

**Commit:** `feat: YAML 1.2 parsing via libfyaml with auto-detect (8 tests)`

---

## Sprint 3: $ref Resolution & JSON Pointer (2-3 weeks)

**Goal:** Complete $ref resolution with cycle detection, JSON Pointer implementation, component references.

### Task 3.1: JSON Pointer (RFC 6901)

**Files:**
- Create: `src/core/oas_jsonptr.h`
- Create: `src/core/oas_jsonptr.c`
- Create: `tests/unit/test_jsonptr.c`
- Modify: `CMakeLists.txt`

**Implementation:**

```c
[[nodiscard]] yyjson_val *oas_jsonptr_resolve(yyjson_val *root, const char *pointer);
[[nodiscard]] int oas_jsonptr_parse(const char *pointer, char ***segments, size_t *count,
                                     oas_arena_t *arena);
```

Per RFC 6901: `~0` → `~`, `~1` → `/`, percent-decode URI fragments.

**Tests (10):**
```c
void test_jsonptr_root(void);                /* "" → root */
void test_jsonptr_simple_key(void);          /* "/foo" */
void test_jsonptr_nested(void);              /* "/foo/bar/baz" */
void test_jsonptr_array_index(void);         /* "/items/0" */
void test_jsonptr_tilde_escape(void);        /* "/a~1b" → key "a/b" */
void test_jsonptr_tilde0_escape(void);       /* "/m~0n" → key "m~n" */
void test_jsonptr_missing_key(void);         /* → nullptr */
void test_jsonptr_empty_key(void);           /* "/" → key "" */
void test_jsonptr_rfc6901_examples(void);    /* all examples from RFC */
void test_jsonptr_parse_segments(void);
```

**Commit:** `feat: JSON Pointer (RFC 6901) resolution with escape handling (10 tests)`

---

### Task 3.2: $ref Resolver

**Files:**
- Create: `src/parser/oas_ref.h`
- Create: `src/parser/oas_ref.c`
- Create: `tests/unit/test_ref.c`
- Modify: `CMakeLists.txt`

**Implementation:**

```c
typedef struct oas_ref_ctx oas_ref_ctx_t;

[[nodiscard]] oas_ref_ctx_t *oas_ref_ctx_create(oas_arena_t *arena, yyjson_val *root);
[[nodiscard]] int oas_ref_resolve(oas_ref_ctx_t *ctx, const char *ref,
                                   yyjson_val **out, oas_error_list_t *errors);
[[nodiscard]] int oas_ref_resolve_all(oas_ref_ctx_t *ctx, oas_doc_t *doc,
                                       oas_error_list_t *errors);
```

Features:
- Local refs: `#/components/schemas/Pet` → JSON Pointer resolution
- Fragment-only: `#Pet` → $anchor resolution
- Cycle detection: visited set, return `-ELOOP` on cycle
- Component shorthand: `#/components/schemas/X`, `#/components/parameters/X`, etc.

**Tests (10):**
```c
void test_ref_local_schema(void);            /* #/components/schemas/Pet */
void test_ref_local_parameter(void);         /* #/components/parameters/limit */
void test_ref_nested(void);                  /* A → B → C chain */
void test_ref_cycle_detection(void);         /* A → B → A → error */
void test_ref_self_reference(void);          /* $ref to self → error */
void test_ref_missing_target(void);          /* nonexistent path → error */
void test_ref_resolve_all_schemas(void);     /* walk entire doc */
void test_ref_fragment_only(void);           /* #/definitions/X */
void test_ref_siblings_preserved(void);      /* 2020-12: $ref siblings valid */
void test_ref_invalid_pointer(void);         /* malformed JSON Pointer */
```

**Commit:** `feat: $ref resolver with cycle detection (10 tests)`

---

## Sprint 4: Schema Compiler (3-4 weeks)

**Goal:** Pre-compile JSON Schema 2020-12 into flat instruction arrays for zero-allocation validation.

### Task 4.1: Instruction Set Design

**Files:**
- Create: `include/liboas/oas_compiler.h`
- Create: `src/compiler/oas_instruction.h`
- Create: `src/compiler/oas_instruction.c`
- Create: `tests/unit/test_instruction.c`
- Modify: `CMakeLists.txt`

**Implementation:**

```c
typedef enum : uint8_t {
    OAS_OP_CHECK_TYPE,       /* check type_mask */
    OAS_OP_CHECK_MIN_LEN,    /* string minLength */
    OAS_OP_CHECK_MAX_LEN,    /* string maxLength */
    OAS_OP_CHECK_PATTERN,    /* regex backend match (PCRE2 or libregexp) */
    OAS_OP_CHECK_FORMAT,     /* format validator */
    OAS_OP_CHECK_MINIMUM,    /* numeric minimum */
    OAS_OP_CHECK_MAXIMUM,    /* numeric maximum */
    OAS_OP_CHECK_MULTIPLE,   /* multipleOf */
    OAS_OP_CHECK_ENUM,       /* enum membership */
    OAS_OP_CHECK_CONST,      /* const equality */
    OAS_OP_CHECK_REQUIRED,   /* required properties */
    OAS_OP_CHECK_MIN_ITEMS,  /* array minItems */
    OAS_OP_CHECK_MAX_ITEMS,  /* array maxItems */
    OAS_OP_CHECK_UNIQUE,     /* uniqueItems */
    OAS_OP_ENTER_OBJECT,     /* descend into object property */
    OAS_OP_ENTER_ARRAY,      /* descend into array items */
    OAS_OP_ENTER_PROPERTY,   /* check specific property */
    OAS_OP_BRANCH_ALLOF,     /* all branches must pass */
    OAS_OP_BRANCH_ANYOF,     /* at least one must pass */
    OAS_OP_BRANCH_ONEOF,     /* exactly one must pass */
    OAS_OP_NEGATE,           /* not: invert next result */
    OAS_OP_JUMP,             /* unconditional jump */
    OAS_OP_JUMP_IF_FAIL,     /* jump if last check failed */
    OAS_OP_END,              /* end of program */
} oas_opcode_t;

typedef struct {
    oas_opcode_t op;
    uint8_t type_mask;       /* for CHECK_TYPE */
    uint16_t str_len;        /* for property names */
    union {
        int64_t i64;
        double f64;
        const char *str;
        size_t offset;       /* jump target */
        void *ptr;           /* compiled regex, enum array */
    } operand;
} oas_instruction_t;

typedef struct {
    oas_instruction_t *code;
    size_t count;
    size_t capacity;
} oas_program_t;
```

**Tests (8):**
```c
void test_instruction_sizes(void);
void test_program_create(void);
void test_program_emit_check_type(void);
void test_program_emit_jump(void);
void test_program_patch_jump(void);          /* backpatch jump offset */
void test_program_emit_sequence(void);
void test_opcode_name(void);
void test_program_reset(void);
```

**Commit:** `feat: validation instruction set and program builder (8 tests)`

---

### Task 4.2: Schema Compiler

**Files:**
- Create: `src/compiler/oas_compiler.c`
- Create: `tests/unit/test_compiler.c`
- Modify: `CMakeLists.txt`

**Implementation:**

```c
typedef struct oas_compiled_schema oas_compiled_schema_t;

[[nodiscard]] oas_compiled_schema_t *oas_schema_compile(const oas_schema_t *schema,
                                                         oas_error_list_t *errors);
void oas_compiled_schema_free(oas_compiled_schema_t *compiled);
size_t oas_compiled_schema_instruction_count(const oas_compiled_schema_t *compiled);
```

Compilation walks `oas_schema_t` tree, emits flat `oas_program_t`:
- Type check first (early exit)
- String constraints: minLength → maxLength → pattern → format
- Numeric constraints: minimum → maximum → multipleOf
- Object: iterate required, then properties with sub-programs
- Array: items/prefixItems sub-programs
- Composition: allOf/anyOf/oneOf with branching
- Pre-compile patterns via `oas_regex_backend_t` vtable (PCRE2 default, libregexp optional)

**Tests (12):**
```c
void test_compile_string_type(void);
void test_compile_string_constraints(void);
void test_compile_integer_range(void);
void test_compile_array_items(void);
void test_compile_object_required(void);
void test_compile_object_properties(void);
void test_compile_allof(void);
void test_compile_oneof(void);
void test_compile_anyof(void);
void test_compile_enum(void);
void test_compile_const(void);
void test_compile_nested_object(void);       /* 3 levels deep */
```

**Commit:** `feat: JSON Schema compiler — schema tree to instruction program (12 tests)`

---

### Task 4.3: Format Validators

**Files:**
- Create: `src/validator/oas_format.h`
- Create: `src/validator/oas_format.c`
- Create: `tests/unit/test_format.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Built-in format validators. Each returns `bool` — valid or not.

```c
typedef bool (*oas_format_fn)(const char *value, size_t len);

[[nodiscard]] oas_format_fn oas_format_get(const char *format_name);

/* Built-in validators */
bool oas_format_date(const char *value, size_t len);        /* YYYY-MM-DD */
bool oas_format_date_time(const char *value, size_t len);   /* RFC 3339 */
bool oas_format_time(const char *value, size_t len);        /* HH:MM:SS */
bool oas_format_email(const char *value, size_t len);       /* RFC 5321 */
bool oas_format_uri(const char *value, size_t len);         /* RFC 3986 */
bool oas_format_uri_reference(const char *value, size_t len);
bool oas_format_uuid(const char *value, size_t len);        /* RFC 4122 */
bool oas_format_ipv4(const char *value, size_t len);
bool oas_format_ipv6(const char *value, size_t len);
bool oas_format_hostname(const char *value, size_t len);    /* RFC 1123 */
bool oas_format_byte(const char *value, size_t len);        /* base64 */
bool oas_format_int32(const char *value, size_t len);
bool oas_format_int64(const char *value, size_t len);
```

**Tests (14):**
```c
void test_format_date_valid(void);
void test_format_date_invalid(void);
void test_format_date_leap_year(void);
void test_format_datetime_valid(void);
void test_format_email_valid(void);
void test_format_uri_valid(void);
void test_format_uuid_valid(void);
void test_format_uuid_invalid(void);
void test_format_ipv4_valid(void);
void test_format_ipv4_invalid(void);
void test_format_ipv6_valid(void);
void test_format_hostname_valid(void);
void test_format_byte_valid(void);           /* base64 */
void test_format_get_unknown(void);          /* returns nullptr */
```

**Commit:** `feat: format validators — date, email, uri, uuid, ipv4/6 (14 tests)`

---

### Task 4.4: Regex Backend Abstraction

**Files:**
- Create: `src/core/oas_regex.h`
- Create: `src/core/oas_regex_pcre2.c`
- Create: `src/core/oas_regex_libregexp.c` (optional, CMake option)
- Create: `vendor/libregexp/` (vendored from QuickJS: libregexp.c/h, libunicode.c/h, cutils.c/h)
- Create: `vendor/libregexp/CMakeLists.txt`
- Create: `tests/unit/test_regex.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Vtable abstraction for ECMA-262 `pattern` validation. Two backends.

```c
typedef struct oas_regex_backend oas_regex_backend_t;

struct oas_regex_backend {
    int (*compile)(void *ctx, const char *pattern, void **compiled);
    bool (*match)(void *ctx, void *compiled, const char *value, size_t len);
    void (*free_pattern)(void *ctx, void *compiled);
    void (*destroy)(void *ctx);
    void *ctx;
};

[[nodiscard]] oas_regex_backend_t *oas_regex_pcre2_create(void);
void oas_regex_backend_destroy(oas_regex_backend_t *backend);

#ifdef OAS_HAVE_LIBREGEXP
[[nodiscard]] oas_regex_backend_t *oas_regex_libregexp_create(void);
#endif
```

PCRE2 backend: `pcre2_compile()` with `PCRE2_ALT_BSUX | PCRE2_MATCH_UNSET_BACKREF`, JIT.
libregexp backend: `lre_compile()` to bytecode, `lre_exec()` to match.

CMake:
```cmake
option(OAS_REGEX_STRICT "Use vendored libregexp for 100% ECMA-262 compliance" OFF)
if(OAS_REGEX_STRICT)
    add_subdirectory(vendor/libregexp)
    target_compile_definitions(oas_core PUBLIC OAS_HAVE_LIBREGEXP)
endif()
```

**Tests (10):**
```c
void test_regex_pcre2_create(void);
void test_regex_pcre2_compile_valid(void);
void test_regex_pcre2_compile_invalid(void);
void test_regex_pcre2_match_pass(void);
void test_regex_pcre2_match_fail(void);
void test_regex_pcre2_unicode(void);
void test_regex_pcre2_unanchored(void);      /* patterns are unanchored per JSON Schema */
void test_regex_pcre2_destroy(void);
void test_regex_backend_swap(void);          /* compile with A, verify with B */
void test_regex_vtable_null_safe(void);
```

**Commit:** `feat: regex backend vtable with PCRE2 default and libregexp optional (10 tests)`

---

## Sprint 5: Validation Engine (3-4 weeks)

**Goal:** Execute compiled schemas against JSON values for request/response validation.

### Task 5.1: Validation VM

**Files:**
- Create: `include/liboas/oas_validator.h`
- Create: `src/validator/oas_validator.c`
- Create: `tests/unit/test_validator.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Stack-based virtual machine that executes compiled schema programs.

```c
typedef struct {
    bool valid;
    oas_error_list_t *errors;
} oas_validation_result_t;

[[nodiscard]] int oas_validate(const oas_compiled_schema_t *schema,
                                yyjson_val *value,
                                oas_validation_result_t *result);

/* Convenience: parse JSON string + validate in one call */
[[nodiscard]] int oas_validate_json(const oas_compiled_schema_t *schema,
                                     const char *json, size_t len,
                                     oas_validation_result_t *result);
```

VM loop: fetch instruction → execute → advance PC. Stack for nested objects/arrays.

**Tests (16):**
```c
void test_validate_string_type_pass(void);
void test_validate_string_type_fail(void);
void test_validate_integer_range_pass(void);
void test_validate_integer_range_fail(void);
void test_validate_string_minlength(void);
void test_validate_string_pattern(void);
void test_validate_array_items(void);
void test_validate_array_min_items(void);
void test_validate_object_required_pass(void);
void test_validate_object_required_fail(void);
void test_validate_object_properties(void);
void test_validate_allof_pass(void);
void test_validate_allof_fail(void);
void test_validate_oneof_pass(void);
void test_validate_oneof_fail(void);         /* 0 or 2+ match */
void test_validate_enum(void);
```

**Commit:** `feat: validation VM — execute compiled schemas against JSON (16 tests)`

---

### Task 5.2: Request/Response Validation

**Files:**
- Create: `src/validator/oas_request.h`
- Create: `src/validator/oas_request.c`
- Create: `tests/unit/test_request.c`
- Modify: `CMakeLists.txt`

**Implementation:**

HTTP request/response validation using compiled schemas from OAS operations.

```c
typedef struct {
    const char *method;          /* GET, POST, etc. */
    const char *path;            /* /pets/123 */
    const char *content_type;    /* application/json */
    const char *body;            /* request body */
    size_t body_len;
    /* Headers and query params as key-value arrays */
    const char **header_keys;
    const char **header_values;
    size_t headers_count;
    const char **query_keys;
    const char **query_values;
    size_t query_count;
} oas_http_request_t;

typedef struct {
    int status_code;
    const char *content_type;
    const char *body;
    size_t body_len;
    const char **header_keys;
    const char **header_values;
    size_t headers_count;
} oas_http_response_t;

/* Compile all schemas in a parsed document (must call after ref resolution) */
typedef struct oas_compiled_doc oas_compiled_doc_t;

[[nodiscard]] oas_compiled_doc_t *oas_doc_compile(const oas_doc_t *doc,
                                                    oas_regex_backend_t *regex,
                                                    oas_error_list_t *errors);
void oas_compiled_doc_free(oas_compiled_doc_t *compiled);

[[nodiscard]] int oas_validate_request(const oas_compiled_doc_t *doc,
                                        const oas_http_request_t *req,
                                        oas_validation_result_t *result);
[[nodiscard]] int oas_validate_response(const oas_compiled_doc_t *doc,
                                         const char *path, const char *method,
                                         const oas_http_response_t *resp,
                                         oas_validation_result_t *result);
```

**Tests (10):**
```c
void test_doc_compile_all_schemas(void);
void test_doc_compile_with_refs(void);
void test_request_validate_body_pass(void);
void test_request_validate_body_fail(void);
void test_request_validate_missing_required_param(void);
void test_request_validate_query_param_type(void);
void test_request_validate_header_param(void);
void test_request_validate_unknown_path(void);
void test_response_validate_body_pass(void);
void test_response_validate_wrong_status(void);
void test_response_validate_content_type(void);
void test_request_validate_no_body_required(void);
```

**Commit:** `feat: document compilation and HTTP request/response validation (12 tests)`

---

## Sprint 6: Path Matching & Emitter (2-3 weeks)

**Goal:** URI template path matching with parameter extraction, JSON/YAML spec emission.

### Task 6.1: Path Template Matcher

**Files:**
- Create: `src/core/oas_path_match.h`
- Create: `src/core/oas_path_match.c`
- Create: `tests/unit/test_path_match.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Match request paths against OpenAPI path templates, extract parameters.

```c
typedef struct {
    const char *name;
    const char *value;
} oas_path_param_t;

typedef struct {
    bool matched;
    const char *template_path;       /* matched template */
    oas_path_param_t *params;
    size_t params_count;
} oas_path_match_result_t;

[[nodiscard]] int oas_path_match(const oas_doc_t *doc, const char *request_path,
                                  oas_path_match_result_t *result, oas_arena_t *arena);
```

Algorithm: split both template and request by `/`, match segments. `{param}` matches any non-empty segment. Static segments match exactly. Longest match wins.

**Tests (10):**
```c
void test_path_match_static(void);           /* /pets → /pets */
void test_path_match_param(void);            /* /pets/123 → /pets/{petId} */
void test_path_match_multiple_params(void);  /* /users/1/posts/2 */
void test_path_match_no_match(void);
void test_path_match_param_extraction(void); /* petId=123 */
void test_path_match_trailing_slash(void);
void test_path_match_priority(void);         /* /pets/mine before /pets/{id} */
void test_path_match_root(void);             /* / */
void test_path_match_encoded(void);          /* /pets/hello%20world */
void test_path_match_empty_param(void);      /* /pets// → no match */
```

**Commit:** `feat: path template matcher with parameter extraction (10 tests)`

---

### Task 6.2: JSON Emitter

**Files:**
- Create: `include/liboas/oas_emitter.h`
- Create: `src/emitter/oas_emitter.c`
- Create: `tests/unit/test_emitter.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Serialize `oas_doc_t` back to JSON using yyjson mutable API.

```c
[[nodiscard]] char *oas_doc_emit_json(const oas_doc_t *doc, size_t *out_len,
                                       bool pretty);
void oas_doc_emit_json_free(char *json);

[[nodiscard]] char *oas_schema_emit_json(const oas_schema_t *schema, size_t *out_len,
                                          bool pretty);
```

Round-trip: parse → emit → parse → compare should produce identical model.

**Tests (8):**
```c
void test_emit_minimal_doc(void);
void test_emit_with_paths(void);
void test_emit_with_schemas(void);
void test_emit_pretty(void);
void test_emit_compact(void);
void test_emit_roundtrip(void);              /* parse → emit → parse → compare */
void test_emit_schema_standalone(void);
void test_emit_ref_preserved(void);          /* $ref emitted as-is */
```

**Commit:** `feat: JSON emitter with round-trip fidelity (8 tests)`

---

## Sprint 7: iohttp Integration (3-4 weeks)

**Goal:** Adapter middleware for iohttp — automatic request/response validation and spec serving.

### Task 7.1: iohttp Adapter

**Files:**
- Create: `src/adapter/oas_iohttp.h`
- Create: `src/adapter/oas_iohttp.c`
- Create: `tests/unit/test_iohttp_adapter.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Adapter consumes iohttp route metadata and provides validation middleware.

```c
typedef struct oas_adapter oas_adapter_t;

typedef struct {
    const char *spec_path;           /* path to OpenAPI JSON file */
    bool validate_requests;          /* enable request validation */
    bool validate_responses;         /* enable response validation (dev only) */
    bool serve_spec;                 /* serve spec at /openapi.json */
    bool serve_scalar;               /* serve Scalar UI at /docs */
    const char *spec_url;            /* URL path for spec (default: /openapi.json) */
    const char *docs_url;            /* URL path for docs (default: /docs) */
} oas_adapter_config_t;

[[nodiscard]] oas_adapter_t *oas_adapter_create(const oas_adapter_config_t *config,
                                                  oas_error_list_t *errors);
void oas_adapter_destroy(oas_adapter_t *adapter);

/* Get the parsed+compiled doc for introspection */
const oas_doc_t *oas_adapter_doc(const oas_adapter_t *adapter);
```

Integration with iohttp uses the `io_middleware_t` callback pattern:
- Pre-handler: validate request against matched operation
- Post-handler: validate response (dev mode)
- Static routes: /openapi.json (spec), /docs (Scalar UI)

**Tests (8):**
```c
void test_adapter_create(void);
void test_adapter_config_defaults(void);
void test_adapter_load_spec(void);
void test_adapter_validate_request(void);
void test_adapter_validate_response(void);
void test_adapter_unknown_path(void);        /* passthrough */
void test_adapter_spec_serving(void);
void test_adapter_destroy(void);
```

**Commit:** `feat: iohttp adapter — validation middleware and spec serving (8 tests)`

---

### Task 7.2: Scalar UI Integration

**Files:**
- Create: `src/adapter/oas_scalar.h`
- Create: `src/adapter/oas_scalar.c`
- Create: `tests/unit/test_scalar.c`
- Modify: `CMakeLists.txt`

**Implementation:**

Generate Scalar UI HTML page pointing to the spec URL.

```c
[[nodiscard]] char *oas_scalar_html(const char *spec_url, const char *title,
                                     size_t *out_len);
void oas_scalar_html_free(char *html);
```

Uses CDN-hosted Scalar (`https://cdn.jsdelivr.net/npm/@scalar/api-reference`). HTML is a simple template with spec URL injected.

**Tests (4):**
```c
void test_scalar_html_contains_spec_url(void);
void test_scalar_html_contains_title(void);
void test_scalar_html_valid_html(void);
void test_scalar_html_custom_url(void);
```

**Commit:** `feat: Scalar UI HTML generation for API documentation (4 tests)`

---

## Sprint 8: Fuzz Testing & Hardening (2-3 weeks)

**Goal:** LibFuzzer targets for all parsing code, edge case hardening.

### Task 8.1: Fuzzer Targets

**Files:**
- Create: `tests/fuzz/fuzz_json_parse.c`
- Create: `tests/fuzz/fuzz_schema_parse.c`
- Create: `tests/fuzz/fuzz_ref_resolve.c`
- Create: `tests/fuzz/fuzz_validate.c`
- Create: `tests/fuzz/fuzz_jsonptr.c`
- Create: `tests/fuzz/fuzz_path_match.c`
- Modify: `CMakeLists.txt`

6 fuzz targets covering all input-parsing surfaces.

**Commit:** `test: LibFuzzer targets for all parsing surfaces (6 fuzzers)`

---

### Task 8.2: Integration Tests

**Files:**
- Create: `tests/integration/test_petstore_full.c`
- Create: `tests/integration/test_roundtrip.c`
- Create: `tests/fixtures/petstore-expanded.json`
- Modify: `CMakeLists.txt`

End-to-end tests: load Petstore spec → compile → validate requests → emit → compare.

**Tests (8):**
```c
/* test_petstore_full.c */
void test_petstore_parse(void);
void test_petstore_resolve_refs(void);
void test_petstore_compile_schemas(void);
void test_petstore_validate_create_pet(void);
void test_petstore_validate_get_pet(void);
void test_petstore_validate_invalid_request(void);
/* test_roundtrip.c */
void test_roundtrip_petstore(void);
void test_roundtrip_preserves_refs(void);
```

**Commit:** `test: integration tests — Petstore full lifecycle (8 tests)`

---

### Task 8.3: Quality Pipeline & Finalization

**Steps:**
1. Run full quality pipeline (format, cppcheck, PVS-Studio, CodeChecker)
2. Fix all findings
3. Create PVS-Studio baseline if needed
4. Create CodeChecker baseline if needed
5. Verify all tests pass with sanitizers (ASan, UBSan, MSan)

**Commit:** `chore: quality pipeline clean — zero findings`

---

## Summary

| Sprint | Duration | What | New Tests |
|--------|----------|------|-----------|
| S1 | 2-3 weeks | Core: arena, errors, interning, yyjson | 30 |
| S2 | 3-4 weeks | OAS Model: schema types, parser, document, YAML | 52 |
| S3 | 2-3 weeks | $ref: JSON Pointer, resolver | 20 |
| S4 | 3-4 weeks | Compiler: instruction set, compiler, formats, regex vtable | 44 |
| S5 | 3-4 weeks | Validator: VM, doc compile, request/response | 28 |
| S6 | 2-3 weeks | Path matching, JSON emitter | 18 |
| S7 | 3-4 weeks | iohttp adapter, Scalar UI | 12 |
| S8 | 2-3 weeks | Fuzz targets, integration tests, hardening | 8 + 6 fuzzers |

**Total: ~212 tests + 6 fuzz targets across 8 sprints (~20-28 weeks)**

## Critical Files

**Core infrastructure:**
- `include/liboas/oas_alloc.h` — arena allocator
- `include/liboas/oas_error.h` — error accumulation
- `include/liboas/oas_schema.h` — JSON Schema types (with discriminator)
- `include/liboas/oas_doc.h` — OAS document model
- `include/liboas/oas_compiler.h` — schema compiler + `oas_doc_compile()`
- `include/liboas/oas_validator.h` — validation engine
- `include/liboas/oas_emitter.h` — spec emission
- `include/liboas/oas_parser.h` — document parser
- `src/core/oas_regex.h` — regex backend vtable (PCRE2 + libregexp)
- `src/parser/oas_yaml.h` — YAML 1.2 parsing (optional, libfyaml)
- `vendor/libregexp/` — vendored QuickJS libregexp (optional)

**iohttp integration:**
- `src/adapter/oas_iohttp.h` — adapter middleware
- iohttp side: `src/middleware/io_oas.c` — consumes liboas adapter

**Reference:**
- `.claude/skills/liboas-architecture/SKILL.md` — architecture guide
- `.claude/skills/json-schema-patterns/SKILL.md` — JSON Schema patterns
- `.claude/skills/rfc-reference/SKILL.md` — RFC reference
- `docs/tmp/draft/developing-an-openapi-library-on-modern-c23.md` — research
- `docs/tmp/draft/design-and-development-of-the-liboas-library.md` — design
