# Architecture Overview

liboas is a C23 library for parsing, validating, and serving OpenAPI 3.2 specifications.
It targets Linux exclusively (kernel 6.7+, glibc 2.39+) and is licensed under GPLv3.

## Two-Layer Architecture

```
                        OpenAPI 3.2 JSON/YAML
                               |
                     +---------+---------+
                     |   Layer 1: Parse  |
                     |   (OAS Model)     |
                     +---------+---------+
                               |
                          oas_doc_t
                               |
                     +---------+---------+
                     |  Layer 2: Compile |
                     |  & Validate       |
                     +---------+---------+
                               |
               +---------------+---------------+
               |               |               |
         Validate Req    Validate Resp    Emit JSON
```

### Layer 1 -- OAS Model (parse-time)

Converts an OpenAPI 3.2 document (JSON or YAML) into an in-memory tree of C
structs. All allocations go through a single arena allocator.

| Component         | Header            | Role                                     |
|-------------------|-------------------|------------------------------------------|
| Parser            | `oas_parser.h`    | JSON string/file to `oas_doc_t`          |
| Document model    | `oas_doc.h`       | Structs for paths, operations, responses |
| Schema model      | `oas_schema.h`    | JSON Schema 2020-12 representation       |
| `$ref` resolver   | (internal)        | Cycle-detecting reference resolution     |
| Builder           | `oas_builder.h`   | Code-first document construction         |
| Emitter           | `oas_emitter.h`   | `oas_doc_t` back to JSON string          |

### Layer 2 -- Compiled Runtime (validate-time)

Pre-compiles schema trees into flat instruction arrays, then validates HTTP
requests and responses against the compiled representation.

| Component         | Header            | Role                                      |
|-------------------|-------------------|-------------------------------------------|
| Schema compiler   | `oas_compiler.h`  | Schema tree to instruction bytecode        |
| Regex backend     | `oas_regex.h`     | ECMA-262 pattern matching (vtable)         |
| Validation VM     | `oas_validator.h` | Execute bytecode against JSON values       |
| Path matcher      | (internal)        | Template matching with param extraction    |
| Request validator | `oas_validator.h` | Full HTTP request validation               |
| Response validator| `oas_validator.h` | Full HTTP response validation              |
| Adapter           | `oas_adapter.h`   | Unified facade for middleware integration  |

## Memory Model

### Arena Allocator

All OAS model nodes are allocated from a single `oas_arena_t`. When the
document is no longer needed, one call to `oas_arena_destroy()` frees
everything in O(1).

```c
oas_arena_t *arena = oas_arena_create(0);   /* 64 KiB default blocks */
oas_doc_t *doc = oas_doc_parse(arena, json, len, errors);
/* ... use document ... */
oas_arena_destroy(arena);                    /* frees all model nodes */
```

Arenas grow by allocating new blocks as needed. `oas_arena_reset()` reclaims
all memory without freeing the underlying blocks, useful for reuse.

### Zero-Copy Strings

String fields in `oas_doc_t` and `oas_schema_t` point directly into the yyjson
document buffer. The yyjson document (`doc->_json_doc`) must remain alive while
those strings are referenced. Call `oas_doc_free()` to release the JSON
document once string data is no longer needed.

## Directory Layout

```
include/liboas/     Public API headers
src/core/           Arena allocator, error list, string interning
src/parser/         JSON/YAML parsing, $ref resolution, JSON Pointer
src/model/          Document model construction
src/compiler/       Schema compilation to bytecode
src/validator/      Validation VM, request/response validation
src/emitter/        JSON emission
src/adapter/        iohttp integration adapter
vendor/libregexp/   Vendored QuickJS libregexp (ECMA-262 regex)
tests/unit/         Unity-based unit tests
tests/fuzz/         LibFuzzer fuzz targets
```

## Component Dependency Flow

```
oas_adapter
    |
    +-- oas_parser --> oas_doc (model)
    |                     |
    |                     +-- oas_schema
    |                     +-- $ref resolver
    |
    +-- oas_compiler --> oas_compiled_doc
    |       |
    |       +-- oas_regex (backend vtable)
    |
    +-- oas_validator
    |       |
    |       +-- path matcher
    |       +-- content negotiation (oas_negotiate)
    |
    +-- oas_emitter
    +-- oas_problem (RFC 9457)
```

## Library Dependencies

| Library          | Version | Role                        | License |
|------------------|---------|-----------------------------|---------|
| yyjson           | 0.12+   | JSON parsing (~2.4 GB/s)    | MIT     |
| libfyaml         | 0.9+    | YAML 1.2 parsing (optional) | MIT     |
| QuickJS libregexp| vendored| ECMA-262 regex              | MIT     |
| Unity            | 2.6.1   | Unit test framework         | MIT     |

## Design Principles

- **Pure C23**: no C++ dependencies, `-std=c23` required.
- **Arena-first**: one allocation strategy for the entire model lifetime.
- **Zero-copy**: string pointers into the original JSON buffer.
- **Compile-then-validate**: schemas are pre-compiled once, validated many times.
- **Vtable abstraction**: regex backend is pluggable via function pointers.
- **Negative errno returns**: all fallible functions return 0 on success or
  `-ENOMEM`, `-EINVAL`, `-ENOENT` on failure.
- **Linux-only**: no Windows or macOS portability burden.
