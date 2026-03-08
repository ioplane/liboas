# liboas - Project Instructions for Claude Code

## Quick Facts

- **Language**: C23 (ISO/IEC 9899:2024), `-std=c23`, `CMAKE_C_EXTENSIONS OFF`
- **Project**: OpenAPI 3.2 library (parsing, validation, serving)
- **License**: GPLv3
- **Status**: Pre-release, first release: 0.1.0
- **Platform**: Linux only (kernel 6.7+, glibc 2.39+)

## Build Commands

```bash
# CMake (primary build system)
cmake --preset clang-debug              # Configure with Clang debug
cmake --build --preset clang-debug      # Build
ctest --preset clang-debug              # Run tests

# Code quality
cmake --build --preset clang-debug --target format       # clang-format
cmake --build --preset clang-debug --target format-check # format check
cmake --build --preset clang-debug --target cppcheck     # static analysis
cmake --build --preset clang-debug --target docs         # Doxygen
```

## Dev Container

- **Image**: `localhost/iohttp-dev:latest` (shared with iohttp project)
- **Compilers**: Clang 22.1.0 (primary), GCC 15.1.1 (gcc-toolset-15, validation)
- **System GCC**: 14.3.1 (OL10 default)
- **Linker**: mold (debug), lld (release)
- **Key tools**: CMake 4.2.3, Unity 2.6.1, cppcheck, Doxygen 1.16.1
- **ALL development/compilation MUST happen inside the podman container**

## Compiler Strategy (dual-compiler)

- **Clang 22+**: Primary dev (MSan, LibFuzzer, clang-tidy, fast builds with mold)
- **GCC 15+**: Validation & release (LTO, -fanalyzer, unique warnings)
- Always use `-std=c23` explicitly for both compilers

## Key Directories

```
include/liboas/     # Public API headers (oas_doc.h, oas_schema.h, oas_validator.h, ...)
src/core/           # Memory allocation, error handling, string interning
src/parser/         # JSON/YAML parsing, $ref resolution
src/model/          # OAS document model (paths, operations, schemas, components)
src/compiler/       # Schema pre-compilation to validation bytecode
src/validator/      # Request/response validation engine
src/emitter/        # JSON/YAML spec emission
src/adapter/        # iohttp integration adapter
tests/unit/         # Unity-based unit tests (test_*.c)
tests/integration/  # Integration tests (full spec parsing + validation)
tests/fuzz/         # LibFuzzer targets (Clang only)
docs/en/            # English documentation
docs/ru/            # Russian documentation
docs/tmp/           # Archived/draft documents (in .gitignore)
examples/           # Example applications
scripts/            # Build and quality scripts
deploy/podman/      # Container configurations
```

## Code Conventions

- **Naming**: `oas_module_verb_noun()` functions, `oas_module_name_t` types, `OAS_MODULE_VALUE` enums/macros
- **Prefix**: `oas_` for all public API
- **Typedef suffix**: `_t` for all types
- **Include guards**: `LIBOAS_MODULE_FILE_H`
- **Pointer style**: `int *ptr` (right-aligned, Linux kernel style)
- **Column limit**: 100 characters
- **Braces**: Linux kernel style (`BreakBeforeBraces: Linux`)
- **Includes order**: `_GNU_SOURCE` -> matching header -> C stdlib -> POSIX -> third-party
- **No C++ dependencies**: Pure C ecosystem only
- **Errors**: return negative errno (`-ENOMEM`, `-EINVAL`), use `goto cleanup` for multi-resource
- **Allocation**: always `sizeof(*ptr)`, never `sizeof(type)`
- **Comments**: Doxygen `@param`/`@return` in headers only; inline comments explain WHY, not WHAT
- **Tests**: Unity framework, `test_module_action_expected()`, typed assertions, cleanup resources

### C23 (mandatory)

| Use everywhere | Use when appropriate | Avoid |
|----------------|---------------------|-------|
| `nullptr`, `[[nodiscard]]`, `constexpr`, `bool` keyword, `_Static_assert` | `typeof`, `[[maybe_unused]]`, `<stdckdint.h>`, digit separators, `unreachable()` | `auto` inference, `_BitInt`, `#embed` |

## Library Stack

| Library   | Version | Role                           |
|-----------|---------|--------------------------------|
| yyjson    | 0.12+   | JSON parsing (~2.4 GB/s)       |
| PCRE2     | 10.40+  | Regex pattern validation       |
| libyaml   | 0.2+    | YAML parsing (optional)        |
| Unity     | 2.6.1   | Unit test framework            |

## Testing Rules

- All new code MUST have unit tests (Unity framework)
- Test files: `tests/unit/test_<module>.c`
- Sanitizers: ASan+UBSan (every commit), MSan (Clang, every commit)
- Fuzzing: LibFuzzer targets in `tests/fuzz/` (Clang only)
- Coverage target: >= 80%

## Post-Sprint Quality Pipeline (MANDATORY)

After completing each sprint, run the **full quality pipeline** inside the container:

```bash
podman run --rm --security-opt seccomp=unconfined \
  -v /opt/projects/repositories/liboas:/workspace:Z \
  localhost/iohttp-dev:latest bash -c "cd /workspace && ./scripts/quality.sh"
```

## Architecture Decisions (DO NOT CHANGE)

- Two-layer architecture: OAS Model (parse) + Compiled Runtime (validate)
- yyjson for JSON parsing (not cJSON, not jansson)
- Pure C libraries only (no C++ dependencies)
- Linux only
- GPLv3 license
- iohttp integration via adapter pattern (not tight coupling)

## Git Workflow

- Branch naming: `feature/description`, `fix/issue-description`
- Commit style: conventional commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`)
- All commits must pass: clang-format, clang-tidy, unit tests
- **NEVER mention "Claude" or any AI assistant in commit messages, comments, or code**

## MCP Documentation (context7)

Use context7 to fetch up-to-date documentation:
- yyjson JSON: `/ibireme/yyjson`
- CMake build: `/websites/cmake_cmake_help`

## OpenAPI References

- OpenAPI 3.2 Specification
- JSON Schema 2020-12 (RFC draft-bhutton-json-schema-01)
- JSON Pointer (RFC 6901)
- URI Reference (RFC 3986)
