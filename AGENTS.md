# liboas - Agent Contract

Single source of truth for AI agents (Claude Code, Codex, Gemini CLI) and human
contributors working on liboas. Declarative, imperative, no narrative.

## Start Here

Read these before making non-trivial changes:

- This file (`AGENTS.md`)
- `.claude/skills/ROADMAP.md`
- Repository-local skills under `.claude/skills/` (see [Skills Reference](#skills-reference))

## Quick Facts

- **Language**: C23 (ISO/IEC 9899:2024), `-std=c23`, `CMAKE_C_EXTENSIONS OFF` (no C extensions)
- **Project**: OpenAPI 3.2 library (parsing, validation, serving)
- **License**: GPLv3
- **Status**: Pre-release; first public release `v0.1.0`
- **Platform**: Linux only (kernel 6.7+, glibc 2.39+)

## Core Working Rules

- Preserve the current public API shape in `include/liboas/` unless the task explicitly requires an API change. Keep public headers stable.
- Keep the two-layer architecture: OAS Model (parse) + Compiled Runtime (validate). See [Architecture Decisions](#architecture-decisions-do-not-change).
- Do not move routing, transport, or application logic into the library core. iohttp integration stays behind the adapter pattern.
- Default to strict behavior. Any lenient behavior must be explicit, documented, and covered by tests.
- Keep zero-copy outputs where possible; avoid hidden allocations in hot paths.
- Use vendored QuickJS libregexp for ECMA-262 regex. Do not introduce PCRE2 or any other regex engine.
- Prefer small, single-purpose translation units. Keep internal-only declarations in `src/`; public declarations in `include/liboas/`.

## Build Commands

CMake is the primary build system.

```bash
# CMake (primary build system)
cmake --preset clang-debug              # Configure with Clang debug
cmake --build --preset clang-debug      # Build
ctest --preset clang-debug              # Run tests

# Code quality targets
cmake --build --preset clang-debug --target format       # clang-format (rewrite)
cmake --build --preset clang-debug --target format-check # format check (verify)
cmake --build --preset clang-debug --target cppcheck     # static analysis
cmake --build --preset clang-debug --target pvs-studio   # PVS-Studio (license in .env)
cmake --build --preset clang-debug --target docs         # Doxygen
```

Run `./scripts/quality.sh` before submitting changes; it builds, tests, checks
formatting, and runs static analysis tools when installed. See
[Post-Sprint Quality Pipeline](#post-sprint-quality-pipeline-mandatory).

## Dev Container

**ALL development/compilation MUST happen inside the podman container.**

- Base image: `podman build -t ioplane-base:latest -f /opt/projects/repositories/container-images/tools/ioplane-base/Containerfile .`
- All `io*` projects share `localhost/ioplane-base:latest` as the common toolchain layer.
- **Image**: `localhost/liboas-dev:latest`
- **Containerfile**: `deploy/podman/Containerfile.dev`

Build and enter the dev container:

```bash
make -C deploy/podman build-dev
podman run --rm -it --security-opt seccomp=unconfined \
  -v $(pwd):/workspace:Z localhost/liboas-dev:latest
```

Toolchain and tools inside the container:

| Component | Version / Notes |
|-----------|-----------------|
| Clang     | 22.1.0 (primary) |
| GCC       | 15.1.1 (gcc-toolset-15, validation) |
| System GCC | 14.3.1 (OL10 default) |
| Linker    | mold (debug), lld (release) |
| CMake     | 4.2.3 |
| Unity     | 2.6.1 |
| cppcheck  | static analysis |
| Doxygen   | 1.16.1 |
| PVS-Studio | proprietary; license in `.env` (copy from `.env.example`), CMake target `pvs-studio` |
| CodeChecker | Clang SA + clang-tidy driver |

## Compiler Strategy (dual-compiler)

- **Clang 22+**: Primary dev (MSan, LibFuzzer, clang-tidy, fast builds with mold).
- **GCC 15+**: Validation & release (LTO, `-fanalyzer`, unique warnings).
- Always pass `-std=c23` explicitly for both compilers.

## Required Host Utilities

Keep these available on the host for effective agent and contributor workflows:

| Utility | Purpose |
|---------|---------|
| `git`   | branches, worktrees, history, patch-oriented review |
| `gh`    | repository and PR automation; requires working `gh api graphql` auth |
| `rg` (`ripgrep`) | fast code and path discovery |
| `jq`    | processing JSON from GitHub APIs, tool output, generated reports |
| `python3` | small project-local automation and validation scripts |
| `podman` | required build/test/quality execution environment |

Strongly recommended / optional:

| Utility | Purpose |
|---------|---------|
| `uv` / `uvx` | optional MCP and helper tooling (e.g. Serena) |
| `clangd` | local semantic C navigation (Serena or other LSP tooling) |
| `fd`    | fast filename discovery |
| `yq`    | YAML inspection |
| `hyperfine` | repeatable benchmark comparisons |

## Key Directories

```
include/liboas/     # Public API headers (oas_doc.h, oas_schema.h, oas_validator.h, ...)
src/core/           # Memory allocation, error handling, string interning, regex, path matching, content negotiation
src/parser/         # JSON/YAML parsing, $ref resolution
src/model/          # OAS document and schema models, builders (paths, operations, schemas, components)
src/compiler/       # Schema pre-compilation to validation bytecode, format validators
src/validator/      # Request/response validation engine, defaults
src/emitter/        # JSON/YAML spec emission
src/adapter/        # iohttp integration adapter
vendor/libregexp/   # Vendored QuickJS libregexp (optional ECMA-262 regex)
tests/unit/         # Unity-based unit tests (test_*.c)
tests/integration/  # Integration tests (full spec parsing + validation)
tests/fuzz/         # LibFuzzer targets (Clang only)
tests/fixtures/     # Test fixtures
docs/en/            # English documentation (authoritative)
docs/ru/            # Russian documentation (translation/adaptation)
docs/tmp/           # Archived/draft scratch documents (in .gitignore, non-authoritative)
examples/           # Example applications
scripts/            # Build and quality scripts
deploy/podman/      # Container configurations
```

## Coding Style & Naming Conventions

Follow `.clang-format`.

- **Naming**: `oas_module_verb_noun()` functions, `oas_module_name_t` types, `OAS_MODULE_VALUE` enums/macros
- **Prefix**: `oas_` for all public API functions; `OAS_` for macros and enum values
- **Typedef suffix**: `_t` for all types
- **Include guards**: `LIBOAS_MODULE_FILE_H`
- **Indentation**: 4 spaces
- **Pointer style**: right-aligned / Linux kernel style (`int *ptr`)
- **Column limit**: 100 characters
- **Braces**: Linux kernel style (`BreakBeforeBraces: Linux`)
- **Includes order**: `_GNU_SOURCE` -> matching header -> C stdlib -> POSIX -> third-party
- **No C++ dependencies**: Pure C ecosystem only
- **Errors**: return negative errno (`-ENOMEM`, `-EINVAL`); use `goto cleanup` for multi-resource paths
- **Allocation**: always `sizeof(*ptr)`, never `sizeof(type)`
- **Comments**: Doxygen `@param`/`@return` in headers only; inline comments explain WHY, not WHAT

### C23 (mandatory)

| Use everywhere | Use when appropriate | Avoid |
|----------------|---------------------|-------|
| `nullptr`, `[[nodiscard]]`, `constexpr`, `bool` keyword, `_Static_assert` | `typeof`, `[[maybe_unused]]`, `<stdckdint.h>`, digit separators, `unreachable()` | `auto` inference, `_BitInt`, `#embed` |

## Code and Review Focus

- Public names: `oas_*`; type suffix `_t`; macro/enum prefix `OAS_`.
- Layer ownership / data flow: `parser -> model -> compiler -> validator`.
- Security focus: `$ref` cycle detection, regex DoS prevention, input size limits.

## Library Stack

| Library   | Version | Role                           | License |
|-----------|---------|--------------------------------|---------|
| yyjson    | 0.12+   | JSON parsing (~2.4 GB/s)       | MIT     |
| libfyaml  | 0.9+    | YAML 1.2 parsing (optional)    | MIT     |
| quickjs-ng libregexp | latest | ECMA-262 regex (default, vendored) | MIT |
| Unity     | 2.6.1   | Unit test framework            | MIT     |

**Regex strategy:** OpenAPI `pattern` requires ECMA-262 semantics. quickjs-ng
`libregexp` extracted standalone (7 files, ~50KB compiled) provides 100%
ECMA-262 via `lre_compile()`/`lre_exec()`. Vendored in `vendor/libregexp/` from
the quickjs-ng/quickjs fork (security fixes: CVE-2025-62495, OOB reads).
Abstracted via the `oas_regex_backend_t` vtable.

**YAML:** libfyaml (NOT libyaml). libyaml only supports YAML 1.1; OpenAPI 3.x
requires YAML 1.2.

## Architecture Decisions (DO NOT CHANGE)

- Two-layer architecture: OAS Model (parse) + Compiled Runtime (validate)
- yyjson for JSON parsing (not cJSON, not jansson)
- Regex via `oas_regex_backend_t` vtable (QuickJS libregexp, vendored)
- YAML via libfyaml (NOT libyaml), optional CMake option
- Pure C libraries only (no C++ dependencies)
- Linux only
- GPLv3 license
- iohttp integration via adapter pattern (not tight coupling)

## Testing Guidelines

- All new code MUST have unit tests (Unity framework), enabled through `OAS_BUILD_TESTS`.
- Test files: `tests/unit/test_<area>.c`; register each new test in `CMakeLists.txt`.
- Test naming: `test_module_action_expected()`; use typed assertions; clean up all resources.
- Add fuzz coverage under `tests/fuzz/` (Clang only) when schema or parser edge cases are hard to encode as deterministic unit tests.
- Sanitizers: ASan+UBSan (every commit), MSan (Clang, every commit). Sanitizer presets such as `clang-asan` are preferred for parser or memory-safety changes.
- Validate at least `ctest --preset clang-debug`.
- Coverage target: >= 80%.

## Post-Sprint Quality Pipeline (MANDATORY)

After completing **each task**, run the **full quality pipeline** inside the
container. All 6 steps MUST pass before the task is considered done. Commit the
task only after the pipeline passes. Fix any failures before moving to the next
task.

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

**If the quality pipeline fails:** fix all findings, re-run until 6/6 PASS, then
commit fixes as `chore: quality pipeline fixes — <description>`.

## Required Follow-Through

- Update unit tests when behavior changes.
- Update docs when layer boundaries, defaults, or public API semantics change.
- Update [CHANGELOG.md](CHANGELOG.md) in the same branch (see [Changelog And Versioning](#changelog-and-versioning)).
- Keep `AGENTS.md` and `.claude/skills/` aligned when repository rules change (this file is the single agent contract; there is no separate `CLAUDE.md`/`CODEX.md` rule set).

## Documentation Style

Write English documentation first. Treat `docs/en/*` as authoritative and
`docs/ru/*` as the translation/adaptation layer.

Documentation must use strict technical language:

- Write facts, contracts, limits, ownership rules, and examples.
- Do not write narrative introductions, philosophy sections, or marketing text.
- Keep comparisons measurable and testable.
- Keep non-goals explicit and tied to API or layer boundaries.
- Use Mermaid for diagrams (see [Documentation Diagrams](#documentation-diagrams-mermaid)).
- Use extended GitHub Markdown only where it improves technical readability:
  tables, fenced code blocks with language tags, short blockquote alerts for
  strict warnings, and `<details>` for secondary examples.
- Use badges in every stable document; badge links must point to official
  sources, official repositories, or official project sites.
- Russian documentation must use Russian prose; keep English only for API
  identifiers, macro names, external project names, protocol names, and RFC
  identifiers.

Keep the stable structure aligned with other `io*` projects:

- `docs/README.md`
- `docs/en/README.md`
- `docs/ru/README.md`
- numbered documents in `docs/en/` and `docs/ru/`

`docs/tmp/` is non-authoritative scratch space.

## Documentation Diagrams (Mermaid)

- **All diagrams MUST use Mermaid** — no ASCII art, no text diagrams, no embedded images.
- **Theme compatibility**: Every diagram must render correctly in both dark and light themes. Do NOT set `%%{init: {'theme': '...'}}%%` — GitHub auto-detects the user's theme. Do NOT hardcode colors.
- **Linting**: All Mermaid diagrams must pass `mmdc` (mermaid-cli) validation before commit.
- **Layout rules**:
  - Prefer vertical (TB/TD) over horizontal (LR) layouts — diagrams must fit vertical screens.
  - Maximum width: ~800px equivalent (avoid horizontal scrolling).
  - No visual overlaps or edge crossings — use subgraphs and ranking to enforce clean layout.
  - Symmetrical and aesthetically balanced node placement.
- **Consistency**: Same visual style across all documentation files (node shapes, edge styles, subgraph colors via theme variables).
- **Diagram type selection**: Choose the most appropriate type for each concept:

| Concept | Diagram type |
|---------|--------------|
| Architecture / components | `graph TD`, `block-beta`, `architecture-beta`, or `flowchart` |
| Data flow / pipelines | `graph TD` with styled edges |
| Interaction / handoff / sequences / protocols | `sequenceDiagram` |
| State machines / transitions | `stateDiagram-v2` |
| Class / type hierarchies | `classDiagram` |
| Timelines / phases | `timeline` |
| Release gates or requirements | `requirementDiagram` |
| Classification or topology | `mindmap`, `block`, or `flowchart` |
| Comparison positioning | `quadrantChart` (only when axes are explicit and measurable) |

## Changelog And Versioning

Maintain [CHANGELOG.md](CHANGELOG.md) for every merge that changes behavior,
public API, verification evidence, release automation, or published
documentation.

Changelog rules:

- Follow [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).
- Keep `Unreleased` at the top.
- Add entries only under the standard sections that apply: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`.
- Write short factual bullets. Do not write narrative paragraphs in the changelog.
- Record externally visible changes only. Do not list routine refactors unless they change behavior, contract, or release evidence.
- Update the changelog in the same branch as the code or documentation change. Do not defer it to a later cleanup branch.

Versioning rules:

- Use Semantic Versioning tags with a leading `v`.
- Current pre-1.0 policy:
  - `v0.y.z` for normal unstable releases
  - `v0.y.z-rc.N` for release candidates
- Do not create `1.0.0` until public API, release gate, and published evidence are all declared stable.
- The first public release for this repository is `v0.1.0`.

## Git Workflow

- Branch naming: `feature/description`, `fix/issue-description`.
- Commit style: conventional commits per `CONTRIBUTING.md` — `feat:`, `fix:`, `refactor:`, `test:`, `docs:` with short imperative subjects.
- All commits must pass: clang-format, clang-tidy, unit tests.
- **NEVER mention "Claude" or any AI assistant in commit messages, comments, or code.**

### Commit & Pull Request Guidelines

- Pull requests must describe the behavior change, list the commands run, and note any analyzer results or skipped checks.
- Include sample inputs or output snippets when modifying parsing behavior or public APIs.

## Skills Reference

Repository-local skills live under `.claude/skills/`. They are the closest thing
to project memory for repeated architecture and standards decisions, even when
the active agent is not Claude.

- **`liboas-architecture/`** — Two-layer architecture, directory layout, naming, memory model, iohttp integration (MANDATORY)
- **`json-schema-patterns/`** — JSON Schema 2020-12 validation, type system, `$ref` resolution, compilation strategy (MANDATORY for `src/compiler/`, `src/validator/`)
- **`rfc-reference/`** — RFC index with priority map, key sections for JSON, URI, HTTP, auth
- **`modern-c23/`** — C23 best practices, mandatory features, patterns, anti-patterns, error handling, memory management (MANDATORY for all C code)

## MCP Documentation (context7)

Use context7 to fetch up-to-date documentation:

| Topic | Library ID |
|-------|------------|
| yyjson JSON | `/ibireme/yyjson` |
| libfyaml YAML | `/pantoniou/libfyaml` |
| QuickJS (libregexp source) | `/nicot/quickjs` |
| OpenAPI spec | `/oai/openapi-specification` |
| CMake build | `/websites/cmake_cmake_help` |

## RFC References

Local copies in `docs/rfc/` — see `docs/rfc/README.md` for the full index.

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
