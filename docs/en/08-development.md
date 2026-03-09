# Development Guide

## Prerequisites

All development happens inside the podman dev container. The host only needs:

- **Podman** (or Docker) for running the dev container
- **Git** for version control

The container provides:

| Tool           | Version | Role                              |
|----------------|---------|-----------------------------------|
| Clang          | 22.1.0  | Primary compiler                  |
| GCC            | 15.1.1  | Validation compiler               |
| CMake          | 4.2.3   | Build system                      |
| mold           | latest  | Linker (debug builds)             |
| lld            | latest  | Linker (release builds)           |
| Unity          | 2.6.1   | Unit test framework               |
| cppcheck       | latest  | Static analysis                   |
| PVS-Studio     | latest  | Proprietary static analyzer       |
| CodeChecker    | latest  | Clang SA + clang-tidy integration |
| Doxygen        | 1.16.1  | API documentation generation      |

## Dev Container Setup

Build and start the container:

```bash
# Build the dev image
make -C deploy/podman build-dev

# Run a shell inside the container
podman run --rm -it --security-opt seccomp=unconfined \
    --env-file .env \
    -v $(pwd):/workspace:Z \
    localhost/liboas-dev:latest bash
```

The `.env` file contains the PVS-Studio license key. Copy from `.env.example`
if it does not exist.

## Build Commands

CMake presets are the primary build interface:

```bash
# Configure (inside container)
cmake --preset clang-debug

# Build
cmake --build --preset clang-debug

# Run tests
ctest --preset clang-debug

# Format check
cmake --build --preset clang-debug --target format-check

# Apply formatting
cmake --build --preset clang-debug --target format

# Static analysis
cmake --build --preset clang-debug --target cppcheck

# Generate Doxygen docs
cmake --build --preset clang-debug --target docs
```

### Compiler Presets

| Preset         | Compiler | Linker | Sanitizers       | Use case          |
|----------------|----------|--------|------------------|-------------------|
| `clang-debug`  | Clang 22 | mold   | ASan+UBSan, MSan | Primary development |
| `gcc-debug`    | GCC 15   | mold   | ASan+UBSan       | Validation        |
| `clang-release`| Clang 22 | lld    | None             | Release builds    |
| `gcc-release`  | GCC 15   | lld    | LTO              | Release builds    |

## Test Commands

```bash
# Run all unit tests
ctest --preset clang-debug

# Run a specific test
ctest --preset clang-debug -R test_schema

# Verbose output
ctest --preset clang-debug --output-on-failure
```

Tests use the Unity framework. Test files are in `tests/unit/test_*.c`.

## Quality Pipeline

After each task, run the full 6-step quality pipeline. All steps must pass
before committing.

```bash
podman run --rm --security-opt seccomp=unconfined \
    --env-file .env \
    -v $(pwd):/workspace:Z \
    localhost/liboas-dev:latest bash -c "cd /workspace && ./scripts/quality.sh"
```

### Pipeline Steps

| Step | Tool         | What it checks                          |
|------|-------------|-----------------------------------------|
| 1    | CMake build  | Compilation with all warnings enabled   |
| 2    | ctest        | All unit tests pass                     |
| 3    | clang-format | Code formatting matches `.clang-format` |
| 4    | cppcheck     | Static analysis (null deref, leaks, UB) |
| 5    | PVS-Studio   | Proprietary deep static analysis        |
| 6    | CodeChecker  | Clang Static Analyzer + clang-tidy      |

If a step fails, fix the findings and re-run until all 6 pass.

## Code Conventions

### Naming

- Functions: `oas_module_verb_noun()` (e.g. `oas_schema_add_property()`)
- Types: `oas_module_name_t` (e.g. `oas_schema_t`)
- Enums/macros: `OAS_MODULE_VALUE` (e.g. `OAS_TYPE_STRING`)
- Include guards: `LIBOAS_MODULE_FILE_H`
- Test functions: `test_module_action_expected()` (e.g. `test_schema_parse_string_type()`)

### Error Handling

Return negative errno values from fallible functions:

```c
[[nodiscard]] int oas_schema_add_property(oas_arena_t *arena, oas_schema_t *schema,
                                          const char *name, oas_schema_t *prop_schema)
{
    if (!arena || !schema || !name || !prop_schema)
        return -EINVAL;

    oas_property_t *prop = oas_arena_alloc(arena, sizeof(*prop), _Alignof(*prop));
    if (!prop)
        return -ENOMEM;

    /* ... */
    return 0;
}
```

For functions that acquire multiple resources, use `goto cleanup`:

```c
int do_work(void)
{
    int rc = -ENOMEM;
    char *buf = malloc(4096);
    if (!buf)
        goto cleanup;

    FILE *f = fopen("file.txt", "r");
    if (!f)
        goto cleanup_buf;

    rc = 0;
    /* ... */

cleanup_file:
    fclose(f);
cleanup_buf:
    free(buf);
cleanup:
    return rc;
}
```

### C23 Features (mandatory)

| Feature          | Usage                                   |
|------------------|-----------------------------------------|
| `nullptr`        | Use instead of `NULL` everywhere        |
| `[[nodiscard]]`  | All functions returning allocated memory |
| `constexpr`      | Integer and float constants             |
| `bool`           | Use the keyword, not `_Bool`            |
| `_Static_assert` | Compile-time invariants                 |

### Formatting

- Column limit: 100 characters
- Braces: Linux kernel style
- Pointer style: `int *ptr` (space before `*`)
- Includes order: `_GNU_SOURCE` -> matching header -> C stdlib -> POSIX -> third-party

## Adding New Tests

1. Create `tests/unit/test_<module>.c` (or add to an existing file).
2. Include `<unity.h>` and the header under test.
3. Implement `setUp()` and `tearDown()` functions.
4. Write test functions prefixed with `test_`.
5. Register tests in `main()` using `RUN_TEST()`.
6. Add the test to `tests/unit/CMakeLists.txt`.

```c
#include <unity.h>
#include <liboas/oas_schema.h>

void setUp(void) {}
void tearDown(void) {}

void test_schema_create_returns_nonnull(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    oas_schema_t *schema = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_EQUAL_UINT8(0, schema->type_mask);
    oas_arena_destroy(arena);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_schema_create_returns_nonnull);
    return UNITY_END();
}
```

## Adding Fuzz Targets

Fuzz targets live in `tests/fuzz/` and use LibFuzzer (Clang only).

1. Create `tests/fuzz/fuzz_<target>.c`.
2. Implement `LLVMFuzzerTestOneInput()`.
3. Add to `tests/fuzz/CMakeLists.txt`.

```c
#include <liboas/oas_parser.h>
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    oas_arena_t *arena = oas_arena_create(0);
    oas_doc_t *doc = oas_doc_parse(arena, (const char *)data, size, nullptr);
    if (doc)
        oas_doc_free(doc);
    oas_arena_destroy(arena);
    return 0;
}
```

## CI Integration

The CI pipeline validates specs using external tools:

- **openapi-generator** -- validates that the spec can be used for client/server
  code generation across languages.
- **openapi-style-validator** -- enforces API style conventions (naming,
  descriptions, response codes).

These run as separate CI steps after the quality pipeline passes.

## Git Workflow

- Branch naming: `feature/description`, `fix/issue-description`
- Commit style: conventional commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`)
- All commits must pass the quality pipeline before pushing.
