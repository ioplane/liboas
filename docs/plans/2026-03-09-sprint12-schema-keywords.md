# Sprint 12: Schema Keywords Completeness

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement remaining JSON Schema 2020-12 keywords — patternProperties, minProperties/maxProperties, propertyNames, dependentRequired/dependentSchemas, contains/minContains/maxContains, and discriminator compilation. Full parse → compile → validate pipeline for each.

**Architecture:** Each keyword follows the same pattern: (1) add field to `oas_schema_t`, (2) parse in `oas_schema_parser.c`, (3) emit in `oas_emitter.c`, (4) add opcode to instruction set, (5) compile in `oas_compiler.c`, (6) execute in `oas_validator.c`. All existing infrastructure reused.

**Tech Stack:** C23, yyjson, Unity, libregexp (for patternProperties regex)

**Skills:** `@json-schema-patterns`, `@modern-c23`, `@liboas-architecture`

---

## Task 12.1: patternProperties (Parse + Compile + Validate)

**Files:**
- Modify: `include/liboas/oas_schema.h` — add `pattern_properties`, `pattern_properties_count`
- Modify: `src/parser/oas_schema_parser.c` — parse `patternProperties`
- Modify: `src/emitter/oas_emitter.c` — emit
- Modify: `src/compiler/oas_instruction.h` — add `OAS_OP_CHECK_PATTERN_PROPS`
- Modify: `src/compiler/oas_compiler.c` — compile
- Modify: `src/validator/oas_validator.c` — validate
- Modify: `tests/unit/test_schema_parser.c`, `tests/unit/test_compiler.c`, `tests/unit/test_validator.c`

**Schema type:**
```c
typedef struct {
    const char *pattern;      /**< Regex pattern for property name */
    oas_schema_t *schema;     /**< Schema to validate matching properties */
} oas_pattern_property_t;
```

Add to `oas_schema_t`:
```c
    oas_pattern_property_t *pattern_properties;
    size_t pattern_properties_count;
```

**Validation logic:** For each property in the object, if property name matches any `patternProperties` pattern (via regex backend), validate the value against that pattern's schema.

**Tests (5):**
- `test_parse_pattern_properties` — parse `{"patternProperties": {"^x-": {"type": "string"}}}`
- `test_compile_pattern_properties` — generates correct opcodes
- `test_validate_pattern_props_pass` — `{"x-foo": "bar"}` passes
- `test_validate_pattern_props_fail` — `{"x-foo": 42}` fails (expects string)
- `test_validate_pattern_props_no_match` — `{"name": 42}` passes (pattern doesn't match)

**Commit:** `feat: patternProperties — parse, compile, validate (5 tests)`

---

## Task 12.2: minProperties / maxProperties

**Files:**
- Modify: `include/liboas/oas_schema.h` — add `min_properties`, `max_properties`
- Modify: `src/parser/oas_schema_parser.c`, `src/emitter/oas_emitter.c`
- Modify: `src/compiler/oas_instruction.h` — `OAS_OP_CHECK_MIN_PROPS`, `OAS_OP_CHECK_MAX_PROPS`
- Modify: `src/compiler/oas_compiler.c`, `src/validator/oas_validator.c`
- Modify: tests

Add to `oas_schema_t`:
```c
    int64_t min_properties;  /**< -1 = not set */
    int64_t max_properties;  /**< -1 = not set */
```

Initialize to -1 in `oas_schema_create()`.

**Tests (4):**
- `test_validate_min_properties_pass` — `{"a":1, "b":2}` with `minProperties: 2`
- `test_validate_min_properties_fail` — `{"a":1}` with `minProperties: 2`
- `test_validate_max_properties_pass` — `{"a":1}` with `maxProperties: 2`
- `test_validate_max_properties_fail` — `{"a":1, "b":2, "c":3}` with `maxProperties: 2`

**Commit:** `feat: minProperties/maxProperties — full pipeline (4 tests)`

---

## Task 12.3: propertyNames

**Files:**
- Modify: `include/liboas/oas_schema.h` — add `oas_schema_t *property_names`
- Modify: parser, emitter, compiler, validator

`propertyNames` applies a schema to every property name in the object (as if each name were a JSON string value).

Add to `oas_schema_t`:
```c
    oas_schema_t *property_names;
```

**Validation:** For each property name string, validate it against `property_names` schema.

**Tests (3):**
- `test_validate_property_names_pass` — `{"ab": 1, "cd": 2}` with `propertyNames: {minLength: 2}`
- `test_validate_property_names_fail` — `{"a": 1}` fails minLength
- `test_validate_property_names_pattern` — names must match `^[a-z]+$`

**Commit:** `feat: propertyNames — validate object property names (3 tests)`

---

## Task 12.4: dependentRequired

**Files:**
- Modify: `include/liboas/oas_schema.h` — add dependent required type
- Modify: parser, emitter, compiler, validator

```c
typedef struct {
    const char *property;         /**< If this property is present... */
    const char **required;        /**< ...these must also be present */
    size_t required_count;
} oas_dependent_required_t;
```

Add to `oas_schema_t`:
```c
    oas_dependent_required_t *dependent_required;
    size_t dependent_required_count;
```

**Tests (3):**
- `test_validate_dependent_required_pass` — `{a: 1, b: 2}` with `dependentRequired: {a: ["b"]}`
- `test_validate_dependent_required_fail` — `{a: 1}` missing dependent `b`
- `test_validate_dependent_required_absent` — `{c: 1}` (trigger property absent, no check)

**Commit:** `feat: dependentRequired — conditional property requirements (3 tests)`

---

## Task 12.5: dependentSchemas

**Files:**
- Modify: `include/liboas/oas_schema.h`
- Modify: parser, emitter, compiler, validator

```c
typedef struct {
    const char *property;
    oas_schema_t *schema;
} oas_dependent_schema_t;
```

Add to `oas_schema_t`:
```c
    oas_dependent_schema_t *dependent_schemas;
    size_t dependent_schemas_count;
```

**Validation:** If property X is present, additionally validate the object against the dependent schema.

**Tests (3):**
- `test_validate_dependent_schema_pass` — property present, dependent schema satisfied
- `test_validate_dependent_schema_fail` — property present, dependent schema fails
- `test_validate_dependent_schema_absent` — trigger property absent, schema not applied

**Commit:** `feat: dependentSchemas — conditional schema application (3 tests)`

---

## Task 12.6: contains / minContains / maxContains

**Files:**
- Modify: `include/liboas/oas_schema.h` — add `contains`, `min_contains`, `max_contains`
- Modify: parser, emitter, compiler, validator

Add to `oas_schema_t`:
```c
    oas_schema_t *contains;
    int64_t min_contains;  /**< -1 = not set (default 1 when contains is present) */
    int64_t max_contains;  /**< -1 = not set */
```

**Validation:** Count how many array items match `contains` schema. Must be >= `min_contains` (default 1) and <= `max_contains`.

**Tests (5):**
- `test_validate_contains_pass` — `[1, "a", 2]` contains at least one string
- `test_validate_contains_fail` — `[1, 2, 3]` no strings (contains: {type: string})
- `test_validate_min_contains` — needs at least 2 matches
- `test_validate_max_contains` — at most 1 match
- `test_validate_contains_without_min` — default minContains=1

**Commit:** `feat: contains/minContains/maxContains — array containment validation (5 tests)`

---

## Task 12.7: Discriminator Compilation

**Files:**
- Modify: `src/compiler/oas_instruction.h` — add `OAS_OP_DISCRIMINATOR`
- Modify: `src/compiler/oas_compiler.c` — emit discriminator opcode
- Modify: `src/validator/oas_validator.c` — execute discriminator dispatch
- Modify: tests

**Opcode:** `OAS_OP_DISCRIMINATOR` reads the discriminator property value from the object, looks up the mapping to find the target schema, and validates against that schema only (instead of trying all oneOf/anyOf branches).

**Tests (4):**
- `test_compile_discriminator_emits_opcode` — verify instruction stream
- `test_validate_discriminator_match` — `{petType: "dog", breed: "lab"}` matches Dog schema
- `test_validate_discriminator_mismatch` — property value not in mapping
- `test_validate_discriminator_missing_prop` — discriminator property absent

**Commit:** `feat: discriminator compilation — polymorphic dispatch opcode (4 tests)`

---

## Task 12.8: Trivial Fields (Info.summary, Schema.deprecated)

**Files:**
- Modify: `include/liboas/oas_doc.h` — add `summary` to `oas_info_t`
- Modify: `include/liboas/oas_schema.h` — add `bool deprecated`
- Modify: parser, emitter
- Modify: tests

**Tests (2):**
- `test_parse_info_summary` — `{"summary": "A brief API description"}`
- `test_parse_schema_deprecated` — `{"deprecated": true}` on schema

**Commit:** `feat: Info.summary and Schema.deprecated fields (2 tests)`

---

## Task 12.9: Quality Pipeline

```bash
podman run --rm --security-opt seccomp=unconfined \
  --env-file .env -v $(pwd):/workspace:Z \
  localhost/liboas-dev:latest bash -c "cd /workspace && ./scripts/quality.sh"
```

**Commit:** `chore: quality pipeline clean — Sprint 12`

---

## Summary

| Task | Feature | Tests |
|------|---------|-------|
| 12.1 | patternProperties | 5 |
| 12.2 | minProperties/maxProperties | 4 |
| 12.3 | propertyNames | 3 |
| 12.4 | dependentRequired | 3 |
| 12.5 | dependentSchemas | 3 |
| 12.6 | contains/minContains/maxContains | 5 |
| 12.7 | Discriminator compilation | 4 |
| 12.8 | Info.summary, Schema.deprecated | 2 |
| 12.9 | Quality pipeline | 0 |
| **Total** | | **29** |
