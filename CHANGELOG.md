# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-03-XX

### Added
- OpenAPI 3.2 document parsing (JSON + YAML)
- JSON Schema 2020-12 compilation to validation bytecode
- Request/response validation engine (34 opcodes)
- $ref resolution with cycle detection
- JSON and YAML emission (round-trip)
- Builder API for code-first OpenAPI generation
- iohttp adapter with Scalar UI
- ECMA-262 regex via vendored QuickJS libregexp
- Arena allocator for zero-copy parsing
- Path template matching with parameter extraction
- RFC 9457 Problem Details error responses
- Content negotiation
- 12 format validators (date, email, uri, ipv4, ipv6, uuid, ...)
- 300+ unit tests, 12 LibFuzzer targets
- Dual compiler: Clang 22+ (primary), GCC 15+ (validation)
- 6-step quality pipeline (build, test, format, cppcheck, PVS-Studio, CodeChecker)
