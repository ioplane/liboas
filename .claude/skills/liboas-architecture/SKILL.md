---
name: liboas-architecture
description: Use when implementing any liboas component — provides architecture reference, directory layout, naming conventions, two-layer design, and integration patterns. MANDATORY for all new files and modules.
---

# liboas Architecture Reference

## Two-Layer Architecture

### Layer 1: OAS Model Layer
Parse OpenAPI 3.2 JSON/YAML into an in-memory document model.

**Components:**
- **Parser** (yyjson-based): JSON parse, optional YAML 1.2 via libfyaml (YAML→yyjson conversion)
- **$ref Resolver**: URI-based resolution with cycle detection and remote document fetch/cache
- **Document Model**: Full OAS 3.2 representation — paths, operations, schemas, parameters, responses, components
- **JSON Schema 2020-12**: Schema nodes with composition (allOf/oneOf/anyOf), $dynamicRef, unevaluatedProperties

### Layer 2: Compiled Runtime Layer
Pre-compile schemas to validation bytecode for zero-allocation hot path.

**Components:**
- **Regex Backend**: Vtable `oas_regex_backend_t` — vendored QuickJS libregexp (100% ECMA-262)
- **Schema Compiler**: Walks schema tree, emits flat instruction array — type checks, format validators, constraint checks, regex via backend vtable
- **Document Compiler**: `oas_doc_compile()` — compiles all schemas in a parsed+resolved document
- **Path Matcher**: OpenAPI path template matching with parameter extraction (`/users/{id}`)
- **Request/Response Validator**: Header, query parameter, and body validation using `oas_compiled_doc_t`
- **Spec Emitter**: Serialize document model back to JSON (round-trip fidelity)

## Directory Layout

```
include/liboas/          # Public API headers
  oas_doc.h             # Document model types
  oas_schema.h          # JSON Schema 2020-12 types
  oas_parser.h          # Parser API
  oas_compiler.h        # Schema compiler API
  oas_validator.h       # Validation API
  oas_emitter.h         # Emitter API
  oas_error.h           # Error types and codes
  oas_alloc.h           # Allocator interface
src/core/               # Memory, errors, string interning
src/parser/             # JSON/YAML parsing, tokenization
src/model/              # OAS document model construction
src/compiler/           # Schema pre-compilation
src/validator/          # Runtime validation engine
src/emitter/            # JSON/YAML output
src/adapter/            # iohttp integration
vendor/libregexp/       # Vendored QuickJS libregexp (ECMA-262 regex)
tests/unit/             # Unity tests
tests/integration/      # Full spec tests
tests/fuzz/             # LibFuzzer targets
```

## Naming Conventions

- **Functions**: `oas_module_verb_noun()` (e.g., `oas_doc_parse()`, `oas_schema_compile()`, `oas_validator_check_request()`)
- **Types**: `oas_module_name_t` (e.g., `oas_doc_t`, `oas_schema_t`, `oas_path_item_t`)
- **Enums**: `OAS_MODULE_VALUE` (e.g., `OAS_TYPE_STRING`, `OAS_STATUS_OK`)
- **Macros**: `OAS_MODULE_VALUE` (e.g., `OAS_MAX_PATH_DEPTH`)
- **Include guards**: `LIBOAS_MODULE_FILE_H`

## Memory Model

- **Arena allocator** for document model (parse once, read many)
- **Per-document arena**: all model nodes allocated from single arena, freed together
- **Compiled schemas**: separate allocation, can outlive document
- **Zero-copy strings**: point into yyjson document, don't duplicate
- **Public API**: `oas_alloc_t` interface for custom allocators

## Error Handling

- Return negative errno (`-EINVAL`, `-ENOMEM`, `-ENOENT`)
- `[[nodiscard]]` on all public API functions
- `oas_error_t` struct for detailed parse/validation errors with location info (line, column, JSON Pointer path)
- Error accumulation: validator collects all errors, not just first

## Integration Pattern (iohttp adapter)

- Adapter consumes `io_router_walk()` to discover routes
- Maps `io_route_opts_t.oas_operation` to compiled validators
- Middleware: pre-handler validates request, post-handler validates response
- Zero-copy: validator reads directly from request headers/body buffers

## Key Data Structures

- `oas_doc_t`: Root document (openapi version, info, servers, paths, components)
- `oas_path_item_t`: Single path with operations map
- `oas_operation_t`: HTTP operation (parameters, requestBody, responses)
- `oas_schema_t`: JSON Schema node (type, properties, items, allOf/oneOf/anyOf, $ref, discriminator)
- `oas_discriminator_mapping_t`: Polymorphic discriminator mapping (propertyName + $ref map)
- `oas_ref_t`: $ref reference (resolved pointer, cycle marker)
- `oas_regex_backend_t`: Regex vtable — compile/match/free, QuickJS libregexp implementation
- `oas_compiled_schema_t`: Pre-compiled validation program (flat instruction array)
- `oas_compiled_doc_t`: All schemas in a document compiled together (owns regex backend)
- `oas_validation_result_t`: Validation outcome (ok/errors list)
