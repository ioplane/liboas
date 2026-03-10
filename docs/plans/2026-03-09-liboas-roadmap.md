# liboas Roadmap: Bugfixes, OAS Coverage, and Framework Backlog

> **Date:** 2026-03-10 (updated)
> **Status:** 0.1.0 DONE (389 tests, 12 fuzz targets, ~10.7K LOC, quality 6/6 PASS)
> **Base:** Sprints 1-16 complete, all merged to master

**Detailed Sprint Plans:**
- [`2026-03-09-sprint10-bugfixes.md`](2026-03-09-sprint10-bugfixes.md) — Codex review bugfixes (8 tests)
- [`2026-03-09-sprint11-oas-core-coverage.md`](2026-03-09-sprint11-oas-core-coverage.md) — OAS core: security, server vars, components (23 tests)
- [`2026-03-09-sprint12-schema-keywords.md`](2026-03-09-sprint12-schema-keywords.md) — Schema keywords completeness (29 tests)
- [`2026-03-09-sprint13-framework-core.md`](2026-03-09-sprint13-framework-core.md) — Framework core: headers, query, RFC 9457 (31 tests)

---

## Part 1: Codex Review Bugfixes (Sprint 10)

Three bugs found by Codex on PR #1. All verified by code review.

### Bug 1 (P1): `compile_media_types()` silently swallows compilation failures

**File:** `src/compiler/oas_doc_compiler.c:91-93`
**Problem:** When `oas_schema_compile()` returns NULL for a schema, the loop `continue`s without recording an error. The function returns success even when schemas failed to compile. Downstream code assumes all schemas compiled.

**Fix:**
```c
// Before (bug):
oas_compiled_schema_t *cs = oas_schema_compile(config, mt->schema, errors);
if (!cs) {
    continue;  // silent skip
}

// After (fix):
oas_compiled_schema_t *cs = oas_schema_compile(config, mt->schema, errors);
if (!cs) {
    ret = -EINVAL;
    continue;  // accumulate errors, report failure at end
}
```
- Change `compile_media_types()` return type to `int` (negative errno on any failure)
- Propagate failure up through `compile_operation()` → `compile_path_item()` → `oas_doc_compile()`
- `oas_doc_compile()` should return NULL when any schema fails, with errors accumulated in `oas_error_list_t`

**Tests (3):** compile_fails_on_bad_schema, compile_error_propagates_to_doc, compile_partial_failure_accumulates_errors

---

### Bug 2 (P2): IPv6 format validator accepts bare IPv4

**File:** `src/compiler/oas_format.c:361-367`
**Problem:** When a `.` is found during IPv6 parsing, the code walks backward to find the IPv4 start position but never checks that any colon-separated groups preceded it. Result: `"1.2.3.4"` is accepted as valid IPv6.

**Fix:**
```c
// After walking back to find v4_start, add:
if (v4_start == 0 && groups == 0) {
    return false;  // no IPv6 prefix before IPv4 suffix
}
```
Minimum requirement: at least one `:` must precede the IPv4 portion (e.g., `::ffff:1.2.3.4` or `::1.2.3.4`).

**Tests (4):** ipv6_rejects_bare_ipv4, ipv6_accepts_mapped_ipv4, ipv6_accepts_embedded_ipv4, ipv6_rejects_ipv4_no_colons

---

### Bug 3 (P2): Regex backend ownership — use-after-free risk

**File:** `src/compiler/oas_doc_compiler.c:292` and `oas_compiled_doc_free()` at line 374
**Problem:** `oas_doc_compile()` stores `config->regex` pointer directly into `cdoc->regex`, then `oas_compiled_doc_free()` calls `regex->destroy()`. If caller created the regex backend and passes it to multiple compilations, the second compilation gets a freed pointer.

**Fix — option A (recommended): Clone/ref-count the regex backend.**
- Add `oas_regex_backend_t *oas_regex_backend_clone(const oas_regex_backend_t *src)` that bumps a refcount or creates a new instance sharing compiled patterns.
- `oas_compiled_doc_free()` decrements refcount, only destroys when zero.

**Fix — option B (simpler): Transfer ownership explicitly.**
- Document that `oas_doc_compile()` takes ownership of `config->regex`.
- Caller must create a fresh backend per `oas_doc_compile()` call (already the pattern in `test_petstore.c` and `oas_adapter.c`).
- Add `config->regex_owned = true/false` flag. Only destroy when `regex_owned`.

**Recommendation:** Option B — simpler, matches existing usage. Add `bool owns_regex` to `oas_compiled_doc_t`. Default to `true` (current behavior). `oas_adapter.c` already creates fresh backends.

**Tests (3):** compile_doc_owns_regex_by_default, compile_doc_borrowed_regex_not_freed, compile_two_docs_separate_regex_backends

---

### Sprint 10 Summary

| Bug | Priority | LOC estimate | Tests |
|-----|----------|-------------|-------|
| Compile failure propagation | P1 | ~40 | 3 |
| IPv6 bare IPv4 | P2 | ~5 | 4 |
| Regex backend ownership | P2 | ~30 | 3 |
| **Total** | | ~75 | **10** |

---

## Part 2: OAS Specification Coverage Plan

Prioritized by real-world usage frequency and spec compliance impact.

### Tier 1 — DONE in 0.1.0

All Tier 1 items completed in Sprints 11-14:
- 1.1: $ref sibling keywords (Sprint 12)
- 1.2: Components (responses, parameters, requestBodies, headers) (Sprint 11)
- 1.3: Response headers (Sprint 13)
- 1.4: Security requirements (Sprint 11)
- 1.5: Security schemes (Sprint 11)
- 1.6: Server variables (Sprint 11)
- 1.7: Parameter style/explode (Sprint 13)
- 1.8: Query parameter deserialization (Sprint 13)

### Tier 2 — Partially done in 0.1.0

Done in Sprint 12:
- 2.1: patternProperties (parse + compile + validate)
- 2.2: minProperties / maxProperties (parse + compile + validate)
- 2.3: propertyNames (parse + compile + validate)
- 2.4: dependentRequired (parse + compile + validate)
- 2.5: dependentSchemas (parse + compile + validate)
- 2.6: contains / minContains / maxContains (parse + compile + validate)
- 2.7: Discriminator compilation (OAS_OP_DISCRIMINATOR opcode)

Remaining for 0.2.0:

| # | Feature | Scope | LOC est. | Tests est. |
|---|---------|-------|----------|------------|
| 2.8 | **unevaluatedProperties / unevaluatedItems** | Compile + Validate (evaluation tracking) | ~200 | 14 |
| 2.9 | **Encoding Object** (multipart) | Parse + content type mapping | ~80 | 8 |
| 2.10 | **Info.summary** | Trivial field addition | ~10 | 2 |
| 2.11 | **Schema deprecated** | Parse + Emit | ~10 | 2 |
| 2.12 | **externalDocs** | Parse + Emit at doc/operation/tag/schema levels | ~60 | 8 |
| | **Tier 2 remaining** | | **~360** | **34** |

### Tier 3 — Nice-to-have for 0.4.0 (advanced, less common)

| # | Feature | Scope | LOC est. | Tests est. |
|---|---------|-------|----------|------------|
| 3.1 | **Remote $ref** (HTTP/HTTPS fetch) | URI resolution, document cache, cycle detection | ~300 | 16 |
| 3.2 | **File $ref** (`./other.yaml`) | File path resolution, relative URI | ~150 | 10 |
| 3.3 | **$dynamicRef / $dynamicAnchor** | Recursive schema extension | ~200 | 12 |
| 3.4 | **$id / $anchor / $defs** | Schema identification and in-schema definitions | ~100 | 10 |
| 3.5 | **Webhooks** | Parse webhooks map, same as paths | ~60 | 6 |
| 3.6 | **Callbacks** | Parse callback map (runtime expressions) | ~100 | 8 |
| 3.7 | **Links** | Parse Link Object with operationId/Ref | ~80 | 6 |
| 3.8 | **Examples** (component + inline) | Parse Example Object, schema-level examples | ~60 | 6 |
| 3.9 | **contentEncoding / contentMediaType / contentSchema** | String content validation | ~100 | 8 |
| 3.10 | **Additional format validators** | uri-template, json-pointer, duration, regex, idn-* | ~200 | 20 |
| 3.11 | **XML Object** | Parse XML hints for XML serialization | ~60 | 6 |
| | **Tier 3 total** | | **~1410** | **108** |

### Tier 4 — OAS 3.2 New Features (0.5.0+)

| # | Feature | Scope | LOC est. | Tests est. |
|---|---------|-------|----------|------------|
| 4.1 | **`query` HTTP method** | PathItem.query + routing | ~30 | 4 |
| 4.2 | **`additionalOperations`** | Arbitrary HTTP methods | ~50 | 6 |
| 4.3 | **`in: querystring`** | Whole query string parameter | ~40 | 4 |
| 4.4 | **`Server.name`** | Trivial field | ~10 | 2 |
| 4.5 | **`$self`** | Document self-reference URI | ~30 | 4 |
| 4.6 | **Streaming: `itemSchema`** | SSE/JSONL/json-seq validation | ~150 | 10 |
| 4.7 | **`prefixEncoding` / `itemEncoding`** | Multipart positional/items | ~80 | 6 |
| 4.8 | **Response.summary** | Trivial field | ~10 | 2 |
| 4.9 | **Response.description optional** | Remove required check in parser | ~5 | 2 |
| 4.10 | **Discriminator.defaultMapping** | Fallback schema | ~30 | 4 |
| 4.11 | **Discriminator.propertyName optional** | Relax validation | ~10 | 2 |
| 4.12 | **Tag.summary / parent / kind** | Tag hierarchy | ~40 | 6 |
| 4.13 | **SecurityScheme.deprecated** | Trivial field | ~10 | 2 |
| 4.14 | **oauth2MetadataUrl / deviceAuthorization** | OAuth extensions | ~60 | 6 |
| 4.15 | **Components.pathItems / mediaTypes** | Reusable components | ~80 | 8 |
| 4.16 | **Example.dataValue / serializedValue** | New example format | ~40 | 4 |
| 4.17 | **XML.nodeType** | Replaces attribute/wrapped | ~20 | 4 |
| | **Tier 4 total** | | **~695** | **74** |

### Coverage Projection

| Version | Tests (cumulative) | OAS Coverage |
|---------|-------------------|-------------|
| 0.1.0 (done) | 389 | ~75% core, ~50% total |
| 0.2.0 | ~550 | ~90% core, ~70% total |
| 0.3.0 | ~700 | ~95% core, ~85% total |
| 0.4.0 | ~800 | ~98%, full 3.2 support |

---

## Part 3: API Framework Backlog (Huma-Inspired)

liboas as a backend for REST/HTTP API frameworks. Reference: [Huma](https://huma.rocks) features.

### Core Framework Features (0.2.0–0.3.0)

| # | Feature | Description | Priority | Deps |
|---|---------|-------------|----------|------|
| F1 | **Header map in request/response** | `oas_http_request_t` needs `headers` (name/value pairs), `query_params`, `cookies` — currently only `method`, `path`, `content_type`, `body` | P0 | None |
| F2 | **Query parameter parsing** | Deserialize query string into typed values per parameter style/explode | P0 | 1.7, 1.8 |
| F3 | **Header validation** | Validate request/response headers against schema + format | P0 | 1.3, F1 |
| F4 | **Path parameter extraction** | Extract typed values from matched path segments (currently string-only) | P1 | None |
| F5 | **Content negotiation** | `Accept` header matching against operation's response media types | P1 | F1 |
| F6 | **Error response (RFC 9457)** | Built-in `application/problem+json` generation from validation errors | P1 | None |
| F7 | **Multiple content types** | Request body selection by `Content-Type`, response by `Accept` | P1 | F5 |
| F8 | **Operation lookup by method+path** | `oas_adapter_find_operation(adapter, method, path)` — fast compiled path match | P1 | None |
| F9 | **Compiled parameter validators** | Pre-compile parameter schemas like body schemas | P2 | 1.7 |
| F10 | **Default values** | Apply `default` from schema when field absent in request | P2 | None |

### Extended Framework Features (0.3.0–0.5.0)

| # | Feature | Description | Priority | Deps |
|---|---------|-------------|----------|------|
| F11 | **Middleware pipeline** | Pre/post hooks: `oas_middleware_t` vtable with `before_validate`, `after_validate`, `on_error` | P2 | None |
| F12 | **Conditional requests** | `ETag`, `If-None-Match`, `If-Modified-Since`, `304 Not Modified` helpers | P3 | F1 |
| F13 | **SSE streaming validation** | Validate individual events against `itemSchema` (OAS 3.2) | P3 | 4.6 |
| F14 | **Multipart form parsing** | Parse `multipart/form-data` per Encoding Object, validate parts | P2 | 2.9 |
| F15 | **Cookie parameter validation** | Extract and validate cookie parameters | P2 | F1 |
| F16 | **CORS helpers** | Generate `Access-Control-Allow-*` headers from spec | P3 | F1 |
| F17 | **Rate limiting metadata** | Parse `x-ratelimit-*` extensions, expose via adapter | P3 | None |
| F18 | **Scalar UI serving** | Already exists (`oas_scalar_html`). Enhance: versioned CDN, custom themes | P3 | None |
| F19 | **Swagger UI serving** | Alternative docs UI alongside Scalar | P3 | None |
| F20 | **CLI tooling** | `liboas-cli` binary: validate spec, diff specs, generate stub code, lint | P2 | None |

### Code-First Builder Enhancements (0.3.0+)

| # | Feature | Description | Priority |
|---|---------|-------------|----------|
| B1 | **Fluent builder macros** | `OAS_ROUTE("/pets", GET, list_pets)` style macros wrapping builder API | P2 |
| B2 | **Schema inference from C types** | `_Generic`-based type → schema mapping for primitives (`int` → integer, `char*` → string) | P2 |
| B3 | **Builder validation** | Validate built doc against OAS spec before compilation | P2 |
| B4 | **Merge builders** | Combine multiple builder-generated docs (microservice composition) | P3 |
| B5 | **Extension properties** | `x-*` extension parsing and emission | P2 |

### Integration Adapters (0.4.0+)

| # | Feature | Description | Priority |
|---|---------|-------------|----------|
| A1 | **iohttp adapter** | Already exists. Enhance: typed params, error responses, middleware hooks | P1 |
| A2 | **Generic HTTP adapter** | Adapter for raw `fd`-based servers (epoll/io_uring) | P2 |
| A3 | **CGI/FastCGI adapter** | Traditional deployment | P3 |
| A4 | **libmicrohttpd adapter** | Popular C HTTP server library | P3 |
| A5 | **libuv adapter** | Node.js-style event loop | P3 |

### Comparison with Huma Features

| Huma Feature | liboas Status | Plan |
|---|---|---|
| Code-first OpenAPI generation | Builder API exists | B1, B2 enhance it |
| Request validation | Body + headers + query (Sprint 13) | F14 (multipart), F15 (cookies) |
| Response validation | Body + headers (Sprint 14) | Done for core |
| Content negotiation (JSON, CBOR) | Implemented (Sprint 13) | Done |
| Conditional requests (ETag, 304) | Not implemented | F12 |
| Error handling (RFC 9457) | Implemented (Sprint 13) | Done |
| Scalar / Swagger UI | Scalar exists | F18, F19 |
| CLI tooling | Not implemented | F20 |
| Middleware | Not implemented | F11 |
| SSE streaming | Not implemented | F13 (with OAS 3.2) |
| Typed path/query params | Path match + query parsing (Sprint 13) | F4 (typed params) |
| Auto-documentation | Builder → emit exists | B1 enhances UX |

---

## Release Roadmap

### 0.1.0 — DONE (2026-03-10)
- 389 tests, 12 fuzz targets, ~10.7K LOC
- Core parse → compile → validate pipeline
- JSON/YAML parse and emit (libfyaml)
- $ref resolution (local) with sibling keywords
- Security schemes, server variables, all component types
- Header/query parameter validation, content negotiation
- RFC 9457 Problem Details, discriminator compilation
- Builder API, iohttp adapter, Scalar UI
- CMake install (find_package + pkg-config), version API
- GitHub Actions CI, full docs (EN + RU)

### 0.2.0 — Remaining Spec Coverage
- Tier 2 remaining: unevaluatedProperties/Items, Encoding Object, externalDocs
- Tier 3: Remote $ref (HTTP/HTTPS), file $ref, $dynamicRef/$dynamicAnchor
- Tier 3: Webhooks, Callbacks, Links, Examples
- Framework: middleware pipeline, cookie validation, CORS helpers
- Builder enhancements: fluent macros, schema inference, builder validation
- Target: ~550 tests

### 0.3.0 — OAS 3.2 Features + Advanced
- Tier 4 OAS 3.2 features: query method, additionalOperations, streaming
- Additional format validators (uri-template, json-pointer, duration)
- contentEncoding/contentMediaType/contentSchema
- Integration adapters: generic HTTP, libmicrohttpd
- CLI tooling (validate, diff, lint)
- Target: ~700 tests

### 0.4.0 — Full OAS 3.2
- Remaining Tier 4 features
- Performance benchmarks
- SSE streaming validation
- All remaining integration adapters
- Target: ~800 tests, full spec coverage

---

## Key Reference Files

| File | Role |
|---|---|
| `docs/en/09-spec-conformance.md` | Field-by-field coverage matrix |
| `include/liboas/oas_doc.h` | Document model types |
| `include/liboas/oas_schema.h` | Schema types |
| `include/liboas/oas_parser.h` | Parser API |
| `include/liboas/oas_validator.h` | Validation API (needs F1 expansion) |
| `include/liboas/oas_adapter.h` | Framework adapter |
| `src/compiler/oas_doc_compiler.c` | Bug 1 + Bug 3 location |
| `src/compiler/oas_format.c` | Bug 2 location |
| `src/parser/oas_schema_parser.c:283-287` | $ref early return (Tier 1.1) |
