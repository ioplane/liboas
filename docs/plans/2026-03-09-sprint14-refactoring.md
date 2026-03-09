# Sprint 14: Refactoring — Analysis Findings and Code Quality

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Fix all remaining issues from the repository analysis: implement `additionalProperties` validation, response code templates, strict JSON Pointer, $ref error propagation, eliminate struct duplication, and fix double file parse.

**Architecture:** Surgical fixes to existing modules. No new modules. One internal header extraction for shared compiled doc types.

**Tech Stack:** C23, yyjson, Unity

**Skills:** `@liboas-architecture`, `@json-schema-patterns`, `@modern-c23`

---

## Task 14.1: Implement `additionalProperties` Validation in VM

**Deps:** None

**Problem:** Compiler emits `OAS_OP_ENTER_ADDITIONAL` with a compiled sub-schema, but the VM handler calls `skip_to_end()` instead of validating. Any object passes regardless of `additionalProperties` constraint.

**Files:**
- Modify: `src/validator/oas_validator.c` — implement `OAS_OP_ENTER_ADDITIONAL` handler
- Modify: `tests/unit/test_validator.c` — update placeholder test, add new tests

**Implementation:**
The handler must:
1. Check if current value is an object (skip otherwise)
2. Collect set of "known" property names (from preceding `OAS_OP_ENTER_PROPERTY` instructions in the same schema scope — these are stored in the bytecode)
3. For each property in the object NOT in the known set, validate its value against the compiled sub-schema
4. Handle `additional_properties_bool == true` with `additional_properties == nullptr` → no constraint (all allowed)

**Algorithm for collecting known properties:** The VM needs to know which properties are "declared". The compiler emits `OAS_OP_ENTER_PROPERTY` with the property name as a STR operand before the `OAS_OP_ENTER_ADDITIONAL`. So we scan backward through executed properties. However, this is complex. Simpler approach: the compiler should emit the list of known property names as part of the `OAS_OP_ENTER_ADDITIONAL` instruction.

**Revised compiler approach:**
1. Before emitting `OAS_OP_ENTER_ADDITIONAL`, emit a `OAS_OP_NOP` with count of known properties
2. Then emit `OAS_OP_NOP` with each known property name as STR operand
3. Then emit `OAS_OP_ENTER_ADDITIONAL`
4. Then emit compiled sub-schema + `OAS_OP_END`

**VM handler:**
1. Read count of known properties from preceding NOPs
2. For each object property, check if name is in known set
3. For unknown properties: validate value against sub-schema (execute instructions between ENTER_ADDITIONAL and END)
4. If `additional_properties_bool == false` (no sub-schema, `additionalProperties: false`): any unknown property is an error

**New compiler encoding:** Add a new opcode `OAS_OP_CHECK_ADDITIONAL` that takes:
- i64 operand: count of known property names
- Followed by N `OAS_OP_NOP` instructions, each with STR operand (property name)
- Followed by compiled sub-schema + END (or just END if additionalProperties: false)

**Tests (4):**
- `test_additional_properties_false_reject` — `{additionalProperties: false}` with extra property → FAIL
- `test_additional_properties_false_known_pass` — `{properties: {name: string}, additionalProperties: false}` with only `name` → PASS
- `test_additional_properties_schema` — `{additionalProperties: {type: "integer"}}` with int values → PASS, string values → FAIL
- `test_additional_properties_true` — `{additionalProperties: true}` → everything passes

**Commit:** `feat: implement additionalProperties validation in VM (4 tests)`

---

## Task 14.2: Response Status Code Templates (2XX/3XX/4XX/5XX)

**Deps:** None

**Problem:** `find_response()` only matches exact status codes and "default". OpenAPI allows wildcard patterns like "2XX", "3XX" etc.

**Files:**
- Modify: `src/validator/oas_request.c` — extend `find_response()`
- Modify: `tests/unit/test_request.c` — add tests

**Implementation:** After exact match and before "default" fallback, try range match:
```c
/* Range match: "2XX" matches 200-299 */
char range_str[4];
snprintf(range_str, sizeof(range_str), "%dXX", status_code / 100);
for (size_t i = 0; i < op->responses_count; i++) {
    if (strcasecmp(op->responses[i].status_code, range_str) == 0) {
        return &op->responses[i];
    }
}
```

**Tests (3):**
- `test_response_2xx_match` — status 201 matches "2XX" response
- `test_response_exact_over_range` — status 200 prefers "200" over "2XX"
- `test_response_range_over_default` — status 201 prefers "2XX" over "default"

**Commit:** `feat: response status code range matching 2XX/3XX/4XX/5XX (3 tests)`

---

## Task 14.3: Strict JSON Pointer Validation (RFC 6901)

**Deps:** None

**Problem:** `oas_jsonptr_parse()` accepts pointers without leading `/`, violating RFC 6901 Section 3.

**Files:**
- Modify: `src/core/oas_jsonptr.c` — reject invalid pointers
- Modify: `tests/unit/test_jsonptr.c` — add tests for rejection

**Implementation:** Change the permissive `if` to return error:
```c
/* Must start with '/' per RFC 6901, or be empty string */
if (*pointer != '/' && *pointer != '\0') {
    return -EINVAL;
}
```
Note: empty string is a valid JSON Pointer per RFC 6901 (references the whole document).

**Tests (3):**
- `test_jsonptr_no_leading_slash` — `"foo/bar"` → -EINVAL
- `test_jsonptr_empty_string` — `""` → valid (whole document)
- `test_jsonptr_root_slash` — `"/"` → valid (empty key)

**Commit:** `fix: strict RFC 6901 JSON Pointer validation — reject missing leading slash (3 tests)`

---

## Task 14.4: Propagate $ref Errors in not/if/then/else

**Deps:** None

**Problem:** `resolve_schema_refs()` ignores return values from recursive calls on `not`/`if`/`then`/`else` schemas (cast to `(void)`). Errors are accumulated in error list but the function returns 0 even if resolution fails.

**Files:**
- Modify: `src/parser/oas_ref.c` — propagate errors
- Create: `tests/unit/test_ref_conditional.c` — test error propagation
- Modify: `CMakeLists.txt` — add test

**Implementation:** Remove `(void)` casts and propagate errors like allOf/anyOf/oneOf:
```c
if (schema->not_schema) {
    int rc = resolve_schema_refs(ctx, doc, schema->not_schema, errors);
    if (rc < 0) return rc;
}
if (schema->if_schema) {
    int rc = resolve_schema_refs(ctx, doc, schema->if_schema, errors);
    if (rc < 0) return rc;
}
/* same for then_schema, else_schema */
```

**Tests (3):**
- `test_ref_in_not_resolved` — `{not: {$ref: "#/..."}}` resolves correctly
- `test_ref_in_if_then_else` — conditional schemas with $ref resolve correctly
- `test_ref_in_not_bad_ref` — `{not: {$ref: "#/nonexistent"}}` returns error

**Commit:** `fix: propagate $ref resolution errors in not/if/then/else schemas (3 tests)`

---

## Task 14.5: Extract Internal Compiled Doc Header (Eliminate Struct Duplication)

**Deps:** 14.1 (may change struct layout)

**Problem:** `compiled_operation_t`, `compiled_media_type_t`, `compiled_param_t`, `compiled_response_t`, and `struct oas_compiled_doc` are duplicated in three files with "must match layout exactly" comments:
- `src/compiler/oas_doc_compiler.c`
- `src/validator/oas_request.c`
- `src/adapter/oas_adapter.c`

**Files:**
- Create: `src/compiler/compiled_doc_internal.h` — single source of truth
- Modify: `src/compiler/oas_doc_compiler.c` — include internal header, remove struct defs
- Modify: `src/validator/oas_request.c` — include internal header, remove struct defs
- Modify: `src/adapter/oas_adapter.c` — include internal header, remove adapter_ prefixed copies

**Implementation:** Move all shared internal types to one header:
```c
/* src/compiler/compiled_doc_internal.h */
#ifndef LIBOAS_COMPILED_DOC_INTERNAL_H
#define LIBOAS_COMPILED_DOC_INTERNAL_H

#include <liboas/oas_compiler.h>
#include <liboas/oas_regex.h>
#include "core/oas_path_match.h"

typedef struct { ... } compiled_media_type_t;
typedef struct { ... } compiled_param_t;
typedef struct { ... } compiled_response_t;
typedef struct { ... } compiled_operation_t;

struct oas_compiled_doc { ... };

#endif
```

**Tests:** No new tests — this is a pure refactoring. All existing tests must pass unchanged.

**Commit:** `refactor: extract compiled doc internal types to shared header — eliminate 3-way struct duplication`

---

## Task 14.6: Fix oas_doc_parse_file Double Read

**Deps:** None

**Problem:** `oas_doc_parse_file()` calls `oas_json_parse_file()` to validate JSON, frees the result, then reads the file again and calls `oas_doc_parse()`. Wastes I/O and CPU.

**Files:**
- Modify: `src/parser/oas_doc_parser.c` — single-pass parse
- Verify: existing tests pass (no new tests needed — behavior unchanged)

**Implementation:** Instead of parsing twice:
1. Read file once with `oas_json_parse_file()` to get `yyjson_doc`
2. Get the root value and call internal parsing directly (the `oas_doc_parse` function parses JSON text again — refactor to accept `yyjson_val *root`)
3. Or simpler: read file to buffer once, pass to `oas_doc_parse(arena, buf, len, errors)`

Simplest approach: read file to buffer, call `oas_doc_parse`:
```c
oas_doc_t *oas_doc_parse_file(oas_arena_t *arena, const char *path, oas_error_list_t *errors)
{
    /* Read file into buffer */
    FILE *f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc((size_t)size);
    if (!buf) { fclose(f); return nullptr; }
    size_t nread = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if ((long)nread != size) { free(buf); return nullptr; }

    oas_doc_t *doc = oas_doc_parse(arena, buf, (size_t)size, errors);
    free(buf);
    return doc;
}
```

**Commit:** `refactor: oas_doc_parse_file single-pass — eliminate redundant file read`

---

## Task 14.7: Quality Pipeline

Run quality pipeline, fix any findings.

```bash
podman run --rm --security-opt seccomp=unconfined \
  --env-file .env -v /opt/projects/repositories/liboas:/workspace:Z \
  localhost/liboas-dev:latest bash -c "cd /workspace/.worktrees/sprint10 && ./scripts/quality.sh"
```

**Commit:** `chore: quality pipeline clean — Sprint 14`

---

## Summary

| Task | Feature | Tests | Type |
|------|---------|-------|------|
| 14.1 | additionalProperties VM validation | 4 | feat |
| 14.2 | Response 2XX/3XX templates | 3 | feat |
| 14.3 | Strict JSON Pointer (RFC 6901) | 3 | fix |
| 14.4 | $ref error propagation in conditionals | 3 | fix |
| 14.5 | Extract internal header (DRY) | 0 | refactor |
| 14.6 | Single-pass file parse | 0 | refactor |
| 14.7 | Quality pipeline | 0 | chore |
| **Total** | | **13** | |
