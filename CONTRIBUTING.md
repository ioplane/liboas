# Contributing to liboas

Thank you for your interest in contributing to liboas.

## Prerequisites

- **Container**: All development happens inside the liboas-dev container
- **Clang 22+**: Primary compiler (provided by the container)
- **GCC 15+**: Validation compiler (provided by the container)
- **Podman**: For running the dev container

```bash
# Build the dev container
make -C deploy/podman build-dev

# Enter the container
podman run --rm -it --security-opt seccomp=unconfined \
  --env-file .env \
  -v /path/to/liboas:/workspace:Z \
  localhost/liboas-dev:latest bash
```

## Building

All builds use CMake presets inside the dev container:

```bash
# Clang debug (primary)
cmake --preset clang-debug
cmake --build --preset clang-debug

# GCC debug (validation)
cmake --preset gcc-debug
cmake --build --preset gcc-debug

# ASan + UBSan
cmake --preset clang-asan
cmake --build --preset clang-asan
```

See `CMakePresets.json` for the full list of presets.

## Testing

```bash
# Run unit tests
ctest --preset clang-debug

# Run with sanitizers
ctest --preset clang-asan

# Run the full quality pipeline (must pass before submitting)
./scripts/quality.sh
```

The quality pipeline runs 6 steps that all must pass:

1. Build (cmake --preset clang-debug)
2. Unit tests (ctest)
3. clang-format (format-check target)
4. cppcheck (static analysis)
5. PVS-Studio (proprietary static analyzer)
6. CodeChecker (Clang SA + clang-tidy)

## Code Style

- **Standard**: C23 (`-std=c23`), no extensions
- **Brace style**: Linux kernel (`BreakBeforeBraces: Linux`)
- **Column limit**: 100 characters
- **Naming**: `oas_module_verb_noun()` for functions, `oas_module_name_t` for types
- **Prefix**: `oas_` for all public API symbols
- **Pointer style**: `int *ptr` (right-aligned)
- **Errors**: Return negative errno (`-ENOMEM`, `-EINVAL`)
- **Allocation**: Always `sizeof(*ptr)`, never `sizeof(type)`
- **C23 features**: Use `nullptr`, `[[nodiscard]]`, `constexpr`, `bool` keyword

Run the formatter before committing:

```bash
cmake --build --preset clang-debug --target format
```

See `CLAUDE.md` for the complete coding conventions reference.

## Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

- `feat:` — New feature
- `fix:` — Bug fix
- `refactor:` — Code restructuring without behavior change
- `test:` — Adding or updating tests
- `docs:` — Documentation changes
- `ci:` — CI/CD changes
- `chore:` — Maintenance tasks

Examples:

```
feat: add JSON Schema discriminator support
fix: reject bare IPv4 in IPv6 format validator
test: add $ref cycle detection tests
```

## Pull Request Process

1. Fork the repository and create a feature branch (`feature/description`)
2. Make your changes following the code style above
3. Add unit tests for all new code (Unity framework)
4. Run the quality pipeline (`./scripts/quality.sh`) — all 6 steps must pass
5. Submit a pull request against `master`
6. Address review feedback

### Testing Requirements

- All new code must have unit tests
- Test naming: `test_module_action_expected()`
- Sanitizers (ASan, UBSan, MSan) must pass
- Coverage target: >= 80%
