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

- **Image**: `localhost/liboas-dev:latest` (standalone from oraclelinux:10)
- **Containerfile**: `deploy/podman/Containerfile.dev` (build: `make -C deploy/podman build-dev`)
- **Compilers**: Clang 22.1.0 (primary), GCC 15.1.1 (gcc-toolset-15, validation)
- **System GCC**: 14.3.1 (OL10 default)
- **Linker**: mold (debug), lld (release)
- **Key tools**: CMake 4.2.3, Unity 2.6.1, cppcheck, Doxygen 1.16.1, PVS-Studio, CodeChecker
- **PVS-Studio**: License in `.env` (copy from `.env.example`), CMake target `pvs-studio`
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
vendor/libregexp/   # Vendored QuickJS libregexp (optional ECMA-262 regex)
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

| Library   | Version | Role                           | License |
|-----------|---------|--------------------------------|---------|
| yyjson    | 0.12+   | JSON parsing (~2.4 GB/s)       | MIT     |
| libfyaml  | 0.9+    | YAML 1.2 parsing (optional)    | MIT     |
| quickjs-ng libregexp | latest | ECMA-262 regex (default, vendored) | MIT |
| Unity     | 2.6.1   | Unit test framework            | MIT     |

**Regex strategy:** OpenAPI `pattern` requires ECMA-262 semantics. quickjs-ng `libregexp` extracted standalone (7 files, ~50KB compiled) provides 100% ECMA-262 via `lre_compile()`/`lre_exec()`. Vendored in `vendor/libregexp/` from quickjs-ng/quickjs fork (security fixes: CVE-2025-62495, OOB reads). Abstracted via `oas_regex_backend_t` vtable.

**YAML:** libfyaml (NOT libyaml). libyaml only supports YAML 1.1; OpenAPI 3.x requires YAML 1.2.

## Testing Rules

- All new code MUST have unit tests (Unity framework)
- Test files: `tests/unit/test_<module>.c`
- Sanitizers: ASan+UBSan (every commit), MSan (Clang, every commit)
- Fuzzing: LibFuzzer targets in `tests/fuzz/` (Clang only)
- Coverage target: >= 80%

## Post-Sprint Quality Pipeline (MANDATORY)

After completing **each task**, run the **full quality pipeline** inside the container. All 6 steps MUST pass before the task is considered done. Commit the task only after the pipeline passes. Fix any failures before moving to the next task.

```bash
podman run --rm --security-opt seccomp=unconfined \
  --env-file .env \
  -v /opt/projects/repositories/liboas:/workspace:Z \
  localhost/liboas-dev:latest bash -c "cd /workspace && ./scripts/quality.sh"
```

**Pipeline steps (all must PASS):**
1. Build (`cmake --preset clang-debug && cmake --build`)
2. Unit tests (`ctest --preset clang-debug`)
3. clang-format (`format-check` target)
4. cppcheck (static analysis)
5. PVS-Studio (proprietary static analyzer)
6. CodeChecker (Clang SA + clang-tidy)

**If quality pipeline fails:** fix all findings, re-run until 6/6 PASS, then commit fixes as `chore: quality pipeline fixes — <description>`.

## Architecture Decisions (DO NOT CHANGE)

- Two-layer architecture: OAS Model (parse) + Compiled Runtime (validate)
- yyjson for JSON parsing (not cJSON, not jansson)
- Regex via `oas_regex_backend_t` vtable (QuickJS libregexp, vendored)
- YAML via libfyaml (NOT libyaml), optional CMake option
- Pure C libraries only (no C++ dependencies)
- Linux only
- GPLv3 license
- iohttp integration via adapter pattern (not tight coupling)

## Git Workflow

- Branch naming: `feature/description`, `fix/issue-description`
- Commit style: conventional commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`)
- All commits must pass: clang-format, clang-tidy, unit tests
- **NEVER mention "Claude" or any AI assistant in commit messages, comments, or code**

## Skills Reference

See `.claude/skills/` for detailed guidance on:
- **`liboas-architecture/`** — Two-layer architecture, directory layout, naming, memory model, iohttp integration (MANDATORY)
- **`json-schema-patterns/`** — JSON Schema 2020-12 validation, type system, $ref resolution, compilation strategy (MANDATORY for src/compiler/, src/validator/)
- **`rfc-reference/`** — RFC index with priority map, key sections for JSON, URI, HTTP, auth
- **`modern-c23/`** — C23 best practices, mandatory features, patterns, anti-patterns, error handling, memory management (MANDATORY for all C code)

## MCP Documentation (context7)

Use context7 to fetch up-to-date documentation:
- yyjson JSON: `/ibireme/yyjson`
- libfyaml YAML: `/pantoniou/libfyaml`
- PCRE2 regex: `/pcre2project/pcre2`
- QuickJS (libregexp source): `/nicot/quickjs`
- OpenAPI spec: `/oai/openapi-specification`
- CMake build: `/websites/cmake_cmake_help`

## RFC References

Local copies in `docs/rfc/` — see `docs/rfc/README.md` for full index.

**P0 — Core (must-implement):**
- RFC 8259 — JSON
- RFC 6901 — JSON Pointer
- RFC 3986 — URI
- RFC 6570 — URI Template
- RFC 9110 — HTTP Semantics

**P1 — Validation:**
- RFC 9457 — Problem Details for HTTP APIs
- RFC 7578 — multipart/form-data
- RFC 2045/2046 — MIME
- RFC 4648 — Base Encodings

**P2 — Security Schemes:**
- RFC 6749/6750 — OAuth 2.0
- RFC 7519 — JWT
- RFC 7515 — JWS
- RFC 7517 — JWK

**Specifications (non-RFC):**
- OpenAPI 3.2 Specification (https://spec.openapis.org/oas/v3.2.0.html)
- JSON Schema 2020-12 (https://json-schema.org/draft/2020-12/json-schema-core)
- YAML 1.2 Specification (https://yaml.org/spec/1.2.2/)
- ECMA-262 (https://tc39.es/ecma262/) — `pattern` regex semantics
