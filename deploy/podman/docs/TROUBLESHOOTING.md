# Troubleshooting Guide

Common issues and solutions for the liboas development container.

## Quick Diagnostics

```bash
# Check if image exists
podman images | grep liboas

# Check container logs
podman logs <container-id>

# System info
podman info | head -20
```

## Build Issues

### Image build fails at Clang download

**Symptom:**
```
curl: (22) The requested URL returned error: 404
```

**Fix:** Check that `CLANG_VERSION` in `Containerfile.dev` matches an existing LLVM release:
```bash
# Verify release exists
curl -I https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.0
```

### Image build fails at gcc-toolset-15

**Symptom:**
```
No match for argument: gcc-toolset-15-gcc
```

**Fix:** Ensure OL10 appstream repo is enabled:
```bash
podman run --rm oraclelinux:10 bash -c "dnf repolist"
```

### "Disk space full" during build

**Fix:**
```bash
# Free Podman cache
podman system prune -af

# Check disk space
df -h /var/lib/containers
```

The full dev image build requires ~15 GB of temporary space.

### CMake build fails inside container

**Symptom:**
```
CMake Error: The source directory "/workspace" does not appear to contain CMakeLists.txt.
```

**Fix:** Ensure volume is mounted correctly:
```bash
podman run --rm -it \
  -v /opt/projects/repositories/liboas:/workspace:Z \
  localhost/liboas-dev:latest ls /workspace/CMakeLists.txt
```

## Runtime Issues

### Sanitizers fail with "operation not permitted"

**Symptom:**
```
==12345==ERROR: AddressSanitizer: SEGV on unknown address
```

**Fix:** Add `--security-opt seccomp=unconfined`:
```bash
podman run --rm --security-opt seccomp=unconfined \
  -v $(pwd):/workspace:Z \
  localhost/liboas-dev:latest bash
```

ASan/MSan need ptrace access which the default seccomp profile blocks.

### PVS-Studio license error

**Symptom:**
```
PVS-Studio license expired or not found
```

**Fix:** The b4all license is pre-activated in the image. For custom licenses:
```bash
# Create .env from example
cp .env.example .env
# Edit .env with your license

# Pass to container
podman run --rm --env-file .env ...
```

### "Library not found" at runtime

**Symptom:**
```
error while loading shared libraries: libpcre2-8.so.0: cannot open
```

**Fix:** Run `ldconfig` inside the container or rebuild the image:
```bash
podman run --rm localhost/liboas-dev:latest ldconfig -p | grep pcre2
```

If missing, rebuild:
```bash
make -C deploy/podman rebuild-dev
```

## Permission Issues

### "Permission denied" on mounted volume

**Symptom:**
```
cmake: /workspace/build/: Permission denied
```

**Causes:**
1. SELinux blocking access
2. File ownership mismatch

**Fix for SELinux:**
```bash
# Relabel directory
chcon -R -t container_file_t /opt/projects/repositories/liboas

# Or use :Z flag (already in Makefile)
podman run -v /path:/workspace:Z ...
```

**Fix for ownership:**
```bash
# Check ownership
ls -la /opt/projects/repositories/liboas/

# Fix if needed
sudo chown -R $USER:$USER /opt/projects/repositories/liboas/
```

### Build artifacts have wrong ownership

**Symptom:** Files created inside container owned by root on host.

**Fix:** Run container with matching UID:
```bash
podman run --rm -it --userns=keep-id \
  -v $(pwd):/workspace:Z \
  localhost/liboas-dev:latest bash
```

## Quality Pipeline Issues

### CodeChecker finds false positives

**Fix:** Add suppressions to the source or configure `.codechecker.json`:
```c
// NOLINTNEXTLINE(cert-err33-c)
(void)snprintf(buf, sizeof(buf), "%s", msg);
```

### PVS-Studio reports on vendored code

**Fix:** Vendor directories are excluded in `scripts/quality.sh`. If a new vendor path is added, update the exclusion list.

### clang-format check fails

**Fix:** Run the formatter:
```bash
# Inside container
cmake --build --preset clang-debug --target format

# Or from host
make -C deploy/podman format
```

## Performance Issues

### Slow image builds

**Tips:**
- Use Podman build cache (don't `--no-cache` unless necessary)
- Ensure fast internet (downloads ~2 GB of dependencies)
- SSD recommended for `/var/lib/containers`

### Slow compilation inside container

**Tips:**
- Use mold linker (default in clang-debug preset)
- Enable ccache: `CMAKE_C_COMPILER_LAUNCHER=ccache`
- Use `-j$(nproc)` for parallel builds
- Mount a named volume for build cache persistence:
  ```bash
  podman run --rm -it \
    -v liboas-build:/workspace/build:Z \
    -v $(pwd):/workspace/src:Z \
    localhost/liboas-dev:latest bash
  ```

## Getting Help

1. Check this guide
2. Run `make -C deploy/podman info` for environment details
3. File an issue with `podman info` output and error logs
