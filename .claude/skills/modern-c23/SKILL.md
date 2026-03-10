---
name: modern-c23
description: Use when writing ANY C code in this project — C23 best practices, mandatory features, patterns, anti-patterns, error handling, memory management, type safety, and performance idioms. Based on project draft research documents.
---

# Modern C23 Best Practices

Standard: ISO/IEC 9899:2024 (`-std=c23`, `CMAKE_C_EXTENSIONS OFF`).
Dual-compiler: Clang 22+ (primary), GCC 15+ (validation).

---

## 1. Mandatory C23 Features

### `nullptr` — Always use instead of NULL

```c
oas_schema_t *schema = nullptr;       // declaration
if (ptr == nullptr) { ... }           // comparison
buf->data = nullptr;                  // reset after free
```

Why: type-safe (`nullptr_t`), no `0`/`NULL` ambiguity, correct with `_Generic`.

### `[[nodiscard]]` — All error-returning public API

```c
[[nodiscard]] int oas_doc_parse(oas_arena_t *arena, const char *json,
                                 size_t len, oas_error_list_t *errors);
[[nodiscard]] oas_schema_t *oas_schema_create(oas_arena_t *arena);
```

Prevents silent error loss. Caller MUST check return value.
In tests, cast to `(void)` if intentionally ignoring.

### `constexpr` — Compile-time constants

```c
constexpr size_t OAS_ARENA_DEFAULT_BLOCK = 64 * 1024;
constexpr size_t OAS_MAX_REF_DEPTH = 64;
constexpr uint32_t OAS_FNV_OFFSET = 0x811c9dc5;
```

Use for: config limits, buffer sizes, hash seeds, bitmask values.
NOT for: runtime-computed values (use `const` for those).

### `bool` keyword — Native boolean

```c
bool has_minimum;          // struct fields
bool nullable;             // flags
if (schema->read_only) {   // conditions
```

Standard since C23 without `<stdbool.h>` (though including it is harmless).

### `_Static_assert` — Compile-time invariants

```c
static_assert(sizeof(oas_type_t) == 1, "type bitmask must fit in uint8_t");
static_assert(OAS_ARENA_DEFAULT_BLOCK >= 4096, "block too small");
static_assert((OAS_ARENA_DEFAULT_BLOCK & (OAS_ARENA_DEFAULT_BLOCK - 1)) == 0,
              "block size must be power of 2");
```

Use for: struct sizes, alignment, enum ranges, power-of-2 checks.

---

## 2. Use When Appropriate

### `[[maybe_unused]]` — Conditional/debug code

```c
[[maybe_unused]] static void debug_dump_schema(const oas_schema_t *s) {
    #ifdef OAS_DEBUG
    fprintf(stderr, "type_mask=0x%02x\n", s->type_mask);
    #endif
}
```

### `typeof` / `typeof_unqual` — Type-safe macros

```c
#define container_of(ptr, type, member) ({                \
    typeof_unqual(((type *)0)->member) *__mptr = (ptr);   \
    (type *)((char *)__mptr - offsetof(type, member));    \
})
```

Compiler catches type mismatches at compile time.

### `<stdckdint.h>` — Checked integer arithmetic

```c
#include <stdckdint.h>

size_t total;
if (ckd_mul(&total, count, sizeof(oas_schema_t *))) {
    return -EOVERFLOW;
}
```

Use for: allocation size calculations, user-provided counts, index arithmetic.

### Digit separators — Readability

```c
constexpr size_t MAX_DOC_SIZE = 10'000'000;   // 10 MB
constexpr uint32_t FNV_PRIME = 16'777'619;
```

### `unreachable()` — After exhaustive switch

```c
switch (kind) {
    case OAS_ERROR_PARSE:    return "parse";
    case OAS_ERROR_VALIDATE: return "validate";
    // ... all cases ...
}
unreachable();  // tells compiler all cases covered
```

---

## 3. Features to AVOID

| Feature | Why | Use Instead |
|---------|-----|-------------|
| `auto` type inference | Weak, unclear intent | Explicit types always |
| `_BitInt(N)` | Non-portable | `int32_t`, `uint64_t` |
| `#embed` | Complicates build | Runtime file loading |

---

## 4. Error Handling Pattern

### Return negative errno, accumulate errors

```c
// Public API: return negative errno
[[nodiscard]] int oas_schema_add_property(oas_arena_t *arena,
                                           oas_schema_t *schema,
                                           const char *name,
                                           oas_schema_t *prop) {
    if (!arena || !schema || !name || !prop)
        return -EINVAL;

    oas_property_t *p = oas_arena_alloc(arena, sizeof(*p), _Alignof(*p));
    if (!p)
        return -ENOMEM;

    // ...
    return 0;
}
```

### `goto cleanup` for multi-resource functions

```c
int oas_doc_parse_file(oas_arena_t *arena, const char *path, ...) {
    FILE *f = nullptr;
    char *buf = nullptr;
    oas_doc_t *doc = nullptr;
    int ret = 0;

    f = fopen(path, "rb");
    if (!f) { ret = -errno; goto cleanup; }

    buf = malloc(size);
    if (!buf) { ret = -ENOMEM; goto cleanup; }

    // ... work ...

    *out = doc;
    doc = nullptr;  // transfer ownership on success

cleanup:
    free(buf);
    if (f) (void)fclose(f);
    return ret;
}
```

### Error accumulation with JSON Pointer paths

```c
oas_error_list_add(errors, OAS_ERROR_PARSE, "/paths/~1pets/get/parameters/0",
                   "parameter missing 'name' field");
// ~1 encodes / in JSON Pointer (RFC 6901)
```

---

## 5. Memory Management Patterns

### Arena allocator — Parse-time allocation

```c
oas_arena_t *arena = oas_arena_create(0);  // default 64 KiB blocks

// All model objects allocated from arena
oas_schema_t *s = oas_arena_alloc(arena, sizeof(*s), _Alignof(*s));

// Single free destroys everything
oas_arena_destroy(arena);
```

Always `sizeof(*ptr)`, never `sizeof(type)`.

### String interning — Deduplicate repeated strings

```c
const char *type_str = oas_intern_get(intern, "string");
// Pointer equality for fast comparison:
if (s->type_name == type_str) { ... }
```

FNV-1a hash, separate chaining, arena-backed.

### Zero-copy from yyjson

yyjson strings point into document memory. Keep `yyjson_doc` alive while accessing `oas_doc_t` string fields. Never `free()` a yyjson-owned string.

```c
// yyjson_get_str() returns pointer into yyjson_doc memory
schema->title = yyjson_get_str(title_val);  // zero-copy, DO NOT free
```

---

## 6. Type Safety Patterns

### Opaque handles (incomplete types in headers)

```c
// Public header:
typedef struct oas_arena oas_arena_t;  // forward declaration only
oas_arena_t *oas_arena_create(size_t block_size);

// Implementation file:
struct oas_arena {
    struct arena_block *head;
    size_t used;
    // ... private fields
};
```

### Fixed-width enum with explicit underlying type

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
```

`: uint8_t` controls size and ABI. Use bitmask values for type arrays.

### Vtable for pluggable backends

```c
struct oas_regex_backend {
    int  (*compile)(void *ctx, const char *pattern, void **compiled);
    bool (*match)(void *ctx, void *compiled, const char *value, size_t len);
    void (*free_pattern)(void *ctx, void *compiled);
    void (*destroy)(void *ctx);
    void *ctx;
};
```

One vtable, one implementation (QuickJS libregexp for ECMA-262).

---

## 7. Struct Initialization

### Designated initializers — Always use for clarity

```c
oas_schema_t s = {
    .type_mask = OAS_TYPE_STRING,
    .min_length = -1,
    .max_length = -1,
    .min_items = -1,
    .max_items = -1,
};
```

Unmentioned fields are zero-initialized. Self-documenting.

### Zero-initialize, then set

```c
oas_schema_t *s = oas_arena_alloc(arena, sizeof(*s), _Alignof(*s));
if (!s) return nullptr;
memset(s, 0, sizeof(*s));
s->min_length = -1;
s->max_length = -1;
// ...
```

---

## 8. Performance Idioms

### Allocation: `sizeof(*ptr)` always

```c
// CORRECT:
oas_schema_t *s = oas_arena_alloc(arena, sizeof(*s), _Alignof(*s));
oas_schema_t **arr = oas_arena_alloc(arena, sizeof(*arr) * count, _Alignof(*arr));

// WRONG:
oas_schema_t *s = oas_arena_alloc(arena, sizeof(oas_schema_t), ...);
```

Automatically correct if type changes.

### Alignment: explicit `_Alignof`

```c
void *p = oas_arena_alloc(arena, sizeof(*p), _Alignof(*p));
const char **strs = oas_arena_alloc(arena, sizeof(*strs) * n, _Alignof(*strs));
```

### Branch hints (GCC/Clang)

```c
if (__builtin_expect(ptr == nullptr, 0)) {  // unlikely
    return -EINVAL;
}
```

Use sparingly — only in measured hot paths.

### Cache-line alignment for hot structures

```c
struct __attribute__((aligned(64))) hot_data {
    // ...
};
static_assert(sizeof(struct hot_data) <= 64, "must fit in cache line");
```

---

## 9. Include Order

```c
#include "matching_header.h"      // 1. Own header (for .c files)

#include <liboas/oas_schema.h>    // 2. Project public headers

#include <string.h>               // 3. C standard library
#include <stdint.h>
#include <stdbool.h>

#include <unistd.h>               // 4. POSIX headers
#include <sys/types.h>

#include <yyjson.h>               // 5. Third-party
#include <libregexp.h>
```

`_GNU_SOURCE` defined via CMake `target_compile_definitions`, NOT in source files.

---

## 10. Testing Conventions

```c
// Unity framework, test_module_action_expected naming
void test_schema_type_mask_single(void) {
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, oas_type_from_string("string"));
    TEST_ASSERT_EQUAL_UINT8(0, oas_type_from_string(nullptr));
}

// Arena per test via setUp/tearDown
static oas_arena_t *arena;
void setUp(void)    { arena = oas_arena_create(0); }
void tearDown(void) { oas_arena_destroy(arena); arena = nullptr; }
```

- Typed assertions: `TEST_ASSERT_EQUAL_UINT8`, `TEST_ASSERT_EQUAL_INT64`, `TEST_ASSERT_DOUBLE_WITHIN`
- `UNITY_INCLUDE_DOUBLE` for double comparisons
- `(void)` cast for intentionally ignored `[[nodiscard]]` returns in tests
- `-Wno-missing-prototypes` for test functions (Unity convention)

---

## 11. Code Style Quick Reference

| Rule | Example |
|------|---------|
| Column limit | 100 chars |
| Braces | Linux kernel (`if (...) {` on same line) |
| Pointer style | `int *ptr` (space before `*`) |
| Naming: functions | `oas_module_verb_noun()` |
| Naming: types | `oas_module_name_t` |
| Naming: enums | `OAS_MODULE_VALUE` |
| Naming: guards | `LIBOAS_MODULE_FILE_H` |
| Comments | WHY not WHAT; Doxygen in headers only |
| Errors | `-errno` return + `goto cleanup` |
| Allocation | `sizeof(*ptr)` + `_Alignof(*ptr)` |
