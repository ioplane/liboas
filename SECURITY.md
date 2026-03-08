# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in liboas, please report it responsibly.

**Email**: Create a private security advisory via GitHub's Security tab.

**Do NOT** open a public issue for security vulnerabilities.

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.x.x   | Latest only |

## Security Practices

- All code is compiled with `-fstack-protector-strong -D_FORTIFY_SOURCE=3`
- Sanitizers (ASan, UBSan, MSan) run on every commit
- Static analysis: cppcheck, clang-tidy, PVS-Studio, CodeChecker
- Fuzz testing with LibFuzzer for all parsing code
