# Contributing to liboas

## Development Setup

All development happens inside the dev container:

```bash
podman run --rm -it --security-opt seccomp=unconfined \
  -v /opt/projects/repositories/liboas:/workspace:Z \
  localhost/iohttp-dev:latest bash
```

## Build and Test

```bash
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
```

## Code Style

- C23 standard (`-std=c23`)
- Linux kernel brace style
- `oas_` prefix for all public API
- 100-character column limit
- Run `cmake --build --preset clang-debug --target format` before committing

## Testing

- All new code must have unit tests (Unity framework)
- Test naming: `test_module_action_expected()`
- Run with sanitizers: ASan+UBSan, MSan

## Commit Style

Conventional commits: `feat:`, `fix:`, `refactor:`, `test:`, `docs:`
