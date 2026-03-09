# liboas Documentation

liboas is a C23 library for parsing, validating, and serving OpenAPI 3.2 specifications.
It targets Linux (kernel 6.7+, glibc 2.39+) and is licensed under GPLv3.

## Quick Start

```c
#include <liboas/oas_adapter.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char spec[] = "{\"openapi\":\"3.1.0\","
                        "\"info\":{\"title\":\"My API\",\"version\":\"1.0\"},"
                        "\"paths\":{}}";

    oas_adapter_t *adapter = oas_adapter_create(spec, sizeof(spec) - 1, nullptr, nullptr);
    if (!adapter) {
        fprintf(stderr, "Failed to load spec\n");
        return 1;
    }

    /* Validate a request */
    oas_http_request_t req = {
        .method = "GET",
        .path = "/pets",
    };
    oas_arena_t *arena = oas_arena_create(0);
    oas_validation_result_t result = {0};
    oas_adapter_validate_request(adapter, &req, &result, arena);

    if (result.valid) {
        printf("Request is valid\n");
    }

    oas_arena_destroy(arena);
    oas_adapter_destroy(adapter);
    return 0;
}
```

## Documentation Index

| File | Description |
|------|-------------|
| [01-architecture.md](01-architecture.md) | Two-layer architecture, memory model, design decisions |
| [02-oas-model.md](02-oas-model.md) | Document model, parsing pipeline, $ref resolution |
| [03-json-schema.md](03-json-schema.md) | JSON Schema 2020-12 support, type system, keywords |
| [04-schema-compiler.md](04-schema-compiler.md) | Schema-to-bytecode compilation pipeline |
| [05-validator.md](05-validator.md) | Validation VM, request/response validation |
| [06-integration.md](06-integration.md) | Adapter pattern, spec-first and code-first workflows |
| [07-api-reference.md](07-api-reference.md) | Complete public API reference |
| [08-development.md](08-development.md) | Build system, testing, quality pipeline |

## Dependencies

| Library | Version | Role | License |
|---------|---------|------|---------|
| yyjson | 0.12+ | JSON parsing (~2.4 GB/s) | MIT |
| libfyaml | 0.9+ | YAML 1.2 parsing (optional) | MIT |
| QuickJS libregexp | latest | ECMA-262 regex (vendored) | MIT |
| Unity | 2.6.1 | Unit test framework | MIT |

## Standards Compliance

- OpenAPI 3.2 Specification
- JSON Schema 2020-12
- RFC 8259 (JSON), RFC 6901 (JSON Pointer), RFC 3986 (URI)
- RFC 9457 (Problem Details), RFC 9110 (HTTP Semantics)
- ECMA-262 (regex pattern semantics)
