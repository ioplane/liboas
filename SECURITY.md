# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in liboas, please report it responsibly.

**Email**: Create a private security advisory via GitHub's Security tab.

**Do NOT** open a public issue for security vulnerabilities.

## Response SLA

| Severity | Triage | Fix |
|----------|--------|-----|
| Critical | 48 hours | 7 days |
| High | 72 hours | 14 days |
| Medium | 1 week | 30 days |
| Low | 2 weeks | Next release |

We will acknowledge receipt within 48 hours and provide an initial assessment.
Critical vulnerabilities (remote code execution, memory corruption exploitable
from untrusted input) will be fixed within 7 days of confirmed triage.

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.x.x   | Latest only |

## Security Practices

- All code is compiled with `-fstack-protector-strong -D_FORTIFY_SOURCE=3`
- Sanitizers (ASan, UBSan, MSan) run on every commit
- Static analysis: cppcheck, clang-tidy, PVS-Studio, CodeChecker
- Fuzz testing with LibFuzzer for all parsing code
- Vendored dependencies are tracked and updated for CVEs

## Disclosure

We follow coordinated disclosure. Once a fix is available, we will:

1. Release a patched version
2. Publish a security advisory on GitHub
3. Credit the reporter (unless anonymity is requested)
