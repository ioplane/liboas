# liboas Podman Container Infrastructure

Development container for **liboas** — a C23 OpenAPI 3.2 library. Standalone image based on Oracle Linux 10 with dual-compiler toolchain (Clang 22 + GCC 15), full quality pipeline, and all library dependencies.

## Quick Start

```bash
cd deploy/podman

# 1. Build dev image (~15 minutes first time)
make build-dev

# 2. Start interactive shell
make dev

# 3. Inside container — build and test
cmake --preset clang-debug -DOAS_YAML=ON
cmake --build --preset clang-debug
ctest --preset clang-debug --output-on-failure
```

## Container Images

| Image | Containerfile | Purpose | Base |
|-------|--------------|---------|------|
| **liboas-dev** | `Containerfile.dev` | Interactive development, quality pipeline | `oraclelinux:10` |
| **liboas-test** | `Containerfile.test` | Pre-built CI test execution | `liboas-dev` |

### Development Image

Full toolchain for interactive development, debugging, and quality checks.

**Compilers:**
- Clang 22.1.0 (primary — MSan, LibFuzzer, clang-tidy, fast builds with mold)
- GCC 15.1.1 via gcc-toolset-15 (validation — LTO, -fanalyzer, unique warnings)
- System GCC 14.3.1 (OL10 default, used for building tools)

**Libraries:**
- yyjson 0.12.0 — JSON parsing (~2.4 GB/s)
- PCRE2 10.45 — Regex with JIT and Unicode
- libfyaml 0.9 — YAML 1.2 parsing (optional)

**Quality Tools:**
- PVS-Studio 7.41 — Static analysis (b4all license)
- CodeChecker — Clang Static Analyzer + clang-tidy wrapper
- cppcheck 2.20 — Additional static analysis
- clang-format / clang-tidy (from Clang 22)

**Build Tools:**
- CMake 4.2.3
- mold linker (fast debug builds)
- ccache (build acceleration)
- ninja (parallel builds)

**Testing:**
- Unity 2.6.1 — Unit test framework
- ASan + UBSan (Clang/GCC)
- MSan (Clang only)
- LibFuzzer (Clang only)

**Documentation:**
- Doxygen 1.16.1 + Graphviz

### Test Image

Pre-built project for fast test execution in CI pipelines.

```bash
# Build (from project root)
make -C deploy/podman build-test

# Run tests
podman run --rm localhost/liboas-test:latest
```

## Makefile Commands

```bash
make build-dev     # Build development image
make build-test    # Build test image (requires dev)
make dev           # Interactive development shell
make test          # Build test image + run tests
make quality       # Full quality pipeline (6 steps)
make build-run     # Build + test (mount source)
make format        # Check clang-format compliance
make info          # Show image and tool versions
make clean         # Remove all liboas images
make rebuild-dev   # Clean + rebuild from scratch
```

## Usage Patterns

### Development Workflow

```bash
# Start container with source mounted
make dev

# Inside container — edit on host, build in container
cmake --preset clang-debug -DOAS_YAML=ON
cmake --build --preset clang-debug
ctest --preset clang-debug --output-on-failure

# Quality checks
cmake --build --preset clang-debug --target format-check
cmake --build --preset clang-debug --target cppcheck
```

### Quality Pipeline

```bash
# Run full 6-step pipeline (from host)
make quality

# Or from project root
podman run --rm --security-opt seccomp=unconfined \
  --env-file .env \
  -v $(pwd):/workspace:Z \
  localhost/liboas-dev:latest bash -c "cd /workspace && ./scripts/quality.sh"
```

Pipeline steps: build, tests, clang-format, cppcheck, PVS-Studio, CodeChecker.

### GCC Validation Build

```bash
# Inside container
cmake --preset gcc-debug
cmake --build --preset gcc-debug
ctest --preset gcc-debug --output-on-failure
```

### Sanitizers

```bash
# ASan + UBSan (default with clang-debug preset)
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug

# MSan (Clang only, separate build)
cmake -B build-msan -DCMAKE_C_COMPILER=clang \
  -DCMAKE_C_FLAGS="-fsanitize=memory -fno-omit-frame-pointer"
cmake --build build-msan
```

## PVS-Studio

PVS-Studio requires a license key. For open-source projects, the b4all license is pre-activated in the image. For custom licenses, create `.env` in the project root:

```bash
cp .env.example .env
# Edit .env with your PVS-Studio credentials
```

Pass it to the container:
```bash
podman run --rm --env-file .env -v ...
```

## SELinux Notes

All volume mounts use `:Z` for SELinux relabeling. If you encounter permission issues:

```bash
# Relabel project directory
chcon -R -t container_file_t /opt/projects/repositories/liboas
```

## Documentation

- [CONTAINER_ARCHITECTURE.md](docs/CONTAINER_ARCHITECTURE.md) — Design decisions
- [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) — Common issues and fixes

## Prerequisites

```bash
# Oracle Linux 10 / RHEL 10
sudo dnf install -y podman

# ~15 GB disk space for dev image build
# ~2 GB for final image
```
