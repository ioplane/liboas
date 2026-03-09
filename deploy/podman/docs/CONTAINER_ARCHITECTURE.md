# Container Architecture

Design decisions and rationale for the liboas container infrastructure.

## Design Goals

1. **Standalone image** — no dependency on external base images (iohttp-dev, etc.)
2. **Dual-compiler** — Clang 22 (primary) + GCC 15 (validation) in one image
3. **Full quality pipeline** — build, test, format, cppcheck, PVS-Studio, CodeChecker
4. **Reproducible builds** — pinned versions for all tools and libraries
5. **SELinux-compatible** — `:Z` volume labeling, rootless-friendly

## Image Hierarchy

```
oraclelinux:10
    |
    +-> liboas-dev:latest (~2 GB)
            |   Full toolchain, all libraries, quality tools
            |
            +-> liboas-test:latest (~2 GB + project)
                    Pre-built project for fast CI test execution
```

## Why Standalone (Not Based on iohttp-dev)?

The original `Containerfile` used `FROM localhost/iohttp-dev:latest`, which:
- Required building iohttp-dev first (circular dependency for new contributors)
- Included unnecessary libraries (wolfSSL, liburing, llhttp, c-ares, etc.)
- Made version upgrades harder (coordinating across two Containerfiles)

The standalone approach builds everything from `oraclelinux:10`:
- Single `make build-dev` to get a working environment
- Only liboas-relevant dependencies (yyjson, PCRE2, libfyaml)
- Independent version management

## Layer Structure

| Layer | Contents | Size |
|-------|----------|------|
| 1 | OracleLinux 10 base | ~200 MB |
| 2 | Base development tools (gcc, git, make, etc.) | ~400 MB |
| 3 | Clang 22.1.0 (selective install from pre-built) | ~300 MB |
| 4 | GCC 15 (gcc-toolset-15 from OL10 repos) | ~200 MB |
| 5 | Python, graphviz, ninja, ccache, lcov | ~100 MB |
| 6 | CMake 4.2.3 (from source) | ~50 MB |
| 7 | cppcheck, Doxygen, Unity | ~30 MB |
| 8 | mold linker | ~50 MB |
| 9 | yyjson, PCRE2, libfyaml | ~20 MB |
| 10 | CodeChecker, PVS-Studio | ~200 MB |
| 11 | ccache config, ldconfig | ~1 MB |

Total: ~1.5-2 GB (acceptable for a full development environment).

## Compiler Strategy

### Why Two Compilers?

| Aspect | Clang 22 | GCC 15 |
|--------|----------|--------|
| Primary use | Daily development | Validation builds |
| Sanitizers | ASan, UBSan, MSan, TSan | ASan, UBSan, TSan |
| Fuzzing | LibFuzzer | (not available) |
| Static analysis | clang-tidy, clang-sa | -fanalyzer |
| Linker | mold (debug), lld (release) | ld (default) |
| Unique value | MSan, LibFuzzer, faster builds | Unique warnings, LTO |

Both use `-std=c23` — C23 is mandatory for the project.

### Why Clang from Pre-Built Binary?

Building LLVM from source takes 30+ minutes and requires 16+ GB RAM. The pre-built release tarball is ~6 GB extracted, but we selectively copy only needed binaries (~300 MB). This keeps build time reasonable (~15 min total for the image).

### Why GCC from gcc-toolset?

OL10 provides gcc-toolset-15 in the appstream repo. Pre-built, no compilation needed. Installed at `/opt/rh/gcc-toolset-15/root/usr/bin/gcc`, symlinked as `gcc-15`.

## Library Decisions

### yyjson (JSON)
- Fastest C JSON library (~2.4 GB/s)
- Immutable and mutable document APIs
- Zero-copy string access
- Built from source for latest version (0.12.0)

### PCRE2 (Regex)
- JIT compilation for fast pattern matching
- Full Unicode support
- ~95% ECMA-262 compatibility (default backend)
- QuickJS libregexp vendored in-tree for 100% ECMA-262 (optional)

### libfyaml (YAML)
- YAML 1.2 support (required by OpenAPI 3.x)
- NOT libyaml — libyaml only supports YAML 1.1
- Anchor/alias resolution
- Optional dependency (`-DOAS_YAML=ON`)

## Quality Pipeline

The image includes all tools needed for the 6-step quality pipeline:

1. **Build** — `cmake --preset clang-debug && cmake --build --preset clang-debug`
2. **Tests** — `ctest --preset clang-debug --output-on-failure`
3. **clang-format** — `cmake --build --preset clang-debug --target format-check`
4. **cppcheck** — `cmake --build --preset clang-debug --target cppcheck`
5. **PVS-Studio** — proprietary static analyzer (b4all license for open-source)
6. **CodeChecker** — wraps clang-tidy + Clang Static Analyzer

All 6 steps must pass before merging. Run via `./scripts/quality.sh`.

## Security Considerations

- `--security-opt seccomp=unconfined` needed for sanitizers (ASan/MSan use ptrace)
- PVS-Studio license passed via `--env-file .env` (not baked into image)
- SELinux `:Z` flag for volume mounts (exclusive relabeling)
- No secrets in the image itself

## Test Image

`Containerfile.test` extends `liboas-dev` by:
1. Copying project source into `/workspace`
2. Running `cmake --preset clang-debug && cmake --build --preset clang-debug`
3. Default CMD: `ctest --preset clang-debug --output-on-failure`

Use for CI pipelines where you want a self-contained test image:
```bash
podman build -t liboas-test:latest -f Containerfile.test /path/to/liboas
podman run --rm localhost/liboas-test:latest
```
