# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-03-10

### Added
- OpenAPI 3.2 document parsing (JSON + YAML via libfyaml)
- JSON Schema 2020-12 compilation to validation bytecode (34 opcodes)
- Request/response validation engine with discriminator support
- $ref resolution with cycle detection and sibling keywords (OAS 3.1+)
- JSON and YAML emission (round-trip)
- Builder API for code-first OpenAPI generation
- iohttp adapter with Scalar UI
- ECMA-262 regex via vendored QuickJS libregexp
- Arena allocator with zero-copy parsing
- Path template matching with parameter extraction
- RFC 9457 Problem Details error responses
- Content negotiation (Accept header matching)
- Security schemes, server variables, all component types parsing
- Header and query parameter validation
- 12 format validators (date, email, uri, ipv4, ipv6, uuid, ...)
- 389 unit tests, 12 LibFuzzer targets
- Dual compiler: Clang 22+ (primary), GCC 15+ (validation)
- 6-step quality pipeline (build, test, format, cppcheck, PVS-Studio, CodeChecker)
- CMake install targets with find_package(liboas) + pkg-config support
- Version API (compile-time macros + runtime oas_version())
- GitHub Actions CI (build matrix, format, analysis)
- Full documentation in English and Russian (8 chapters each)
