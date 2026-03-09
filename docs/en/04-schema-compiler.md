# Schema Compiler

## Overview

The schema compiler transforms `oas_schema_t` trees into flat instruction arrays
(`oas_program_t`) that the validation VM executes. This eliminates recursive tree
traversal during validation, producing predictable, cache-friendly execution.

## Compilation Pipeline

```
oas_schema_t tree
      |
      v
oas_schema_compile()
      |
      +-- emit type checks
      +-- emit constraint instructions (min/max/pattern/format)
      +-- emit property traversals (ENTER_PROPERTY)
      +-- emit array item traversals (ENTER_ITEMS, ENTER_PREFIX_ITEM)
      +-- emit composition branches (BRANCH_ALLOF/ANYOF/ONEOF)
      +-- emit conditional blocks (COND_IF/THEN/ELSE)
      +-- compile regex patterns via backend
      +-- resolve format validators
      +-- emit OAS_OP_END
      |
      v
oas_compiled_schema_t
  +-- program (instruction array)
  +-- regex backend reference
  +-- compiled patterns array
```

## API

### Single Schema Compilation

```c
oas_compiler_config_t config = {
    .regex = oas_regex_libregexp_create(),
    .format_policy = OAS_FORMAT_ENFORCE,
};
oas_error_list_t *errors = oas_error_list_create(arena);

oas_compiled_schema_t *compiled = oas_schema_compile(schema, &config, errors);
if (!compiled) {
    /* Check errors for compilation failures */
}

/* Use compiled schema for validation... */

oas_compiled_schema_free(compiled);
```

### Full Document Compilation

```c
oas_compiled_doc_t *compiled = oas_doc_compile(doc, &config, errors);
if (!compiled) {
    /* Check errors */
}

/* Compiled doc includes:
 * - Path matcher for URL routing
 * - All operation schemas compiled
 * - Parameter schemas compiled
 * - Request body / response schemas compiled
 */

oas_compiled_doc_free(compiled);
```

## Compiled Schema Structure

```c
struct oas_compiled_schema {
    oas_program_t program;            /* Instruction bytecode */
    oas_regex_backend_t *regex;       /* Regex backend for pattern matching */
    oas_compiled_pattern_t **patterns; /* Pre-compiled regex patterns */
    size_t pattern_count;
    size_t pattern_capacity;
};
```

## Compiled Document Structure

```c
struct oas_compiled_doc {
    oas_path_matcher_t *matcher;       /* Path template matcher */
    compiled_operation_t *operations;   /* Compiled operations */
    size_t operations_count;
    oas_compiled_schema_t **all_schemas; /* All schemas (for cleanup) */
    size_t all_schemas_count;
    oas_regex_backend_t *regex;         /* Shared regex backend */
    bool owns_regex;
    oas_arena_t *arena;                 /* Arena for compiled structures */
};
```

Each `compiled_operation_t` contains:
- Path template and HTTP method
- Operation ID
- Compiled request body schemas by content type
- Compiled response schemas by status code and content type
- Compiled parameter schemas (query, header, path, cookie)

## Instruction Set

The bytecode uses a fixed-size instruction format:

```c
typedef struct {
    oas_opcode_t op;       /* Operation code (uint8_t enum) */
    uint8_t type_mask;     /* Type bitmask for CHECK_TYPE */
    uint16_t _pad;
    union {
        int64_t i64;       /* Integer operands (min/max lengths, counts) */
        double f64;        /* Float operands (minimum/maximum/multipleOf) */
        const char *str;   /* String operands (property names, patterns) */
        size_t offset;     /* Jump targets */
        void *ptr;         /* Compiled pattern pointers, format functions */
        struct {
            uint16_t count;
            uint16_t index;
        } branch;          /* Composition branch metadata */
    } operand;
} oas_instruction_t;
```

### Instruction Reference

**Type and Value Checks:**

| Opcode | Operand | Description |
|--------|---------|-------------|
| `CHECK_TYPE` | `type_mask` | Verify value type matches bitmask |
| `CHECK_ENUM` | `ptr` (yyjson_val*) | Value must be in enum array |
| `CHECK_CONST` | `ptr` (yyjson_val*) | Value must equal const |

**String Constraints:**

| Opcode | Operand | Description |
|--------|---------|-------------|
| `CHECK_MIN_LEN` | `i64` | Unicode codepoint count >= min |
| `CHECK_MAX_LEN` | `i64` | Unicode codepoint count <= max |
| `CHECK_PATTERN` | `ptr` (compiled regex) | String matches ECMA-262 pattern |
| `CHECK_FORMAT` | `ptr` (format function) | String passes format validator |

**Numeric Constraints:**

| Opcode | Operand | Description |
|--------|---------|-------------|
| `CHECK_MINIMUM` | `f64` | value >= minimum |
| `CHECK_MAXIMUM` | `f64` | value <= maximum |
| `CHECK_EX_MINIMUM` | `f64` | value > exclusiveMinimum |
| `CHECK_EX_MAXIMUM` | `f64` | value < exclusiveMaximum |
| `CHECK_MULTIPLE_OF` | `f64` | value % multipleOf == 0 |

**Array Constraints:**

| Opcode | Operand | Description |
|--------|---------|-------------|
| `CHECK_MIN_ITEMS` | `i64` | Array length >= min |
| `CHECK_MAX_ITEMS` | `i64` | Array length <= max |
| `CHECK_UNIQUE` | -- | All elements are distinct |
| `ENTER_ITEMS` | -- | Validate each element against items schema |
| `ENTER_PREFIX_ITEM` | `i64` (index) | Validate element at index |
| `CHECK_CONTAINS` | -- | At least one element matches contains schema |

**Object Constraints:**

| Opcode | Operand | Description |
|--------|---------|-------------|
| `CHECK_REQUIRED` | `str` | Named property must exist |
| `ENTER_PROPERTY` | `str` | Enter property for sub-validation |
| `ENTER_ADDITIONAL` | -- | Validate additional properties |
| `CHECK_MIN_PROPS` | `i64` | Property count >= min |
| `CHECK_MAX_PROPS` | `i64` | Property count <= max |
| `CHECK_PROP_NAMES` | -- | All property names match schema |
| `CHECK_PATTERN_PROPS` | -- | Pattern property validation |
| `CHECK_DEP_REQUIRED` | -- | Dependent required validation |
| `CHECK_DEP_SCHEMA` | -- | Dependent schema validation |

**Composition and Control Flow:**

| Opcode | Operand | Description |
|--------|---------|-------------|
| `BRANCH_ALLOF` | `branch` | All sub-schemas must match |
| `BRANCH_ANYOF` | `branch` | At least one must match |
| `BRANCH_ONEOF` | `branch` | Exactly one must match |
| `NEGATE` | -- | Invert (not) result |
| `COND_IF` | -- | Evaluate if-schema |
| `COND_THEN` | -- | Apply then-schema if condition passed |
| `COND_ELSE` | -- | Apply else-schema if condition failed |
| `DISCRIMINATOR` | `str` | Dispatch by discriminator property |
| `JUMP` | `offset` | Unconditional jump |
| `JUMP_IF_FAIL` | `offset` | Jump if previous check failed |
| `PUSH_SCOPE` | -- | Push evaluation scope |
| `POP_SCOPE` | -- | Pop evaluation scope |
| `CHECK_UNEVAL_PROPS` | -- | Check unevaluated properties |
| `CHECK_UNEVAL_ITEMS` | -- | Check unevaluated items |
| `END` | -- | Program termination |

## Regex Compilation

During compilation, each `pattern` keyword is compiled via the regex backend:

```c
oas_compiled_pattern_t *pat = nullptr;
int rc = config->regex->compile(config->regex, schema->pattern, &pat);
if (rc < 0) {
    /* Invalid regex pattern -- add compilation error */
}
/* pat is stored in the compiled schema and referenced by CHECK_PATTERN instructions */
```

The `borrow_regex` flag in `oas_compiler_config_t` controls ownership:
- `false` (default): compiled schema takes ownership of the regex backend
- `true`: caller retains ownership (useful when sharing one backend across schemas)

## Compiler Configuration

```c
typedef struct {
    oas_regex_backend_t *regex;   /* Regex backend for pattern compilation */
    uint8_t format_policy;        /* OAS_FORMAT_IGNORE / WARN / ENFORCE */
    bool borrow_regex;            /* If true, caller retains regex ownership */
} oas_compiler_config_t;
```
