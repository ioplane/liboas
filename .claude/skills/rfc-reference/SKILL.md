---
name: rfc-reference
description: Use when implementing features that reference IETF standards — JSON, URI, HTTP semantics, authentication, media types. Provides RFC numbers, key sections, and priority mapping for liboas.
---

# RFC Reference for liboas

## P0 — Core (must-implement)

| RFC | Title | Key Sections |
|-----|-------|--------------|
| 8259 | JSON | S1-13: full spec |
| 6901 | JSON Pointer | S3-5: syntax, evaluation |
| 3986 | URI | S3: syntax, S5: resolution |
| 6570 | URI Template | S2-3: expressions, operators |
| 9110 | HTTP Semantics | S8-9: methods, S15: status codes, S8.3: content negotiation |

### RFC 8259 — JSON
- S4: JSON text is a serialized value (object, array, number, string, true, false, null)
- S6: Numbers — no leading zeros, no hex, no NaN/Infinity
- S7: Strings — UTF-8, escape sequences (\uXXXX), surrogate pairs for non-BMP
- S8.1: Duplicate object keys — behavior undefined, liboas uses last-wins

### RFC 6901 — JSON Pointer
- S3: Syntax — `""` is whole document, `/foo/0` is first element of `foo` array
- S4: Evaluation — `~0` encodes `~`, `~1` encodes `/`
- S5: JSON String representation — used in `$ref` fragments

### RFC 3986 — URI
- S3: Components — scheme, authority, path, query, fragment
- S5.3: Component recomposition — building URI from parts
- S5.4: Reference resolution — merging base and relative URIs (critical for $ref)

### RFC 6570 — URI Template
- S2: Template syntax — `{var}`, `{+var}`, `{#var}`, `{.var}`, `{/var}`, `{;var}`, `{?var}`, `{&var}`
- S3: Expression expansion — simple, reserved, fragment, label, path, parameter, query, continuation
- Used in OAS server variables and link objects

### RFC 9110 — HTTP Semantics
- S8: Methods — GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH
- S8.3: Content negotiation — Accept, Accept-Encoding, Accept-Language
- S15: Status codes — 1xx informational, 2xx success, 3xx redirection, 4xx client error, 5xx server error
- S8.6: Content-Type — media type in request/response

## P1 — Validation

| RFC | Title | Key Sections |
|-----|-------|--------------|
| 9457 | Problem Details | S3: members, S6.1: content type |
| 7578 | multipart/form-data | S4: definition, S5: encoding |
| 2045 | MIME | S5: content-type, S6: encodings |
| 4648 | Base Encodings | S4: base64, S8: base64url |
| 6838 | Media Types | S4.2: registration |

### RFC 9457 — Problem Details for HTTP APIs
- S3: Members — type (URI), title, status, detail, instance
- S3.1: Extension members — additional context
- S6.1: Content type — `application/problem+json`
- Validation errors should produce Problem Details responses

### RFC 7578 — multipart/form-data
- S4: Definition — boundary, CRLF-delimited parts
- S4.2: Content-Disposition — `name` parameter identifies field
- S5: Encoding — charset, Content-Transfer-Encoding
- Maps to OAS `requestBody` with `multipart/form-data` media type

### RFC 4648 — Base Encodings
- S4: base64 — A-Z, a-z, 0-9, +, / with = padding
- S5: base64url — A-Z, a-z, 0-9, -, _ (URL-safe variant)
- Used in `format: byte` (base64) and JWT tokens (base64url)

## P2 — Security Schemes

| RFC | Title | Key Sections |
|-----|-------|--------------|
| 7235 | HTTP Auth | S2: challenges, S4: credentials |
| 6750 | Bearer Token | S2: usage, S3: error handling |
| 7519 | JWT | S4: claims, S7: validation |
| 7515 | JWS | S3: compact serialization |
| 7517 | JWK | S4: key parameters |
| 6749 | OAuth 2.0 | S1.3: authorization grant, S4: obtaining access tokens |

### RFC 7235 — HTTP Authentication
- S2: Challenge/response — `WWW-Authenticate` / `Authorization` headers
- S2.1: Authentication scheme — `Basic`, `Bearer`, custom
- S4.1: Credentials — scheme-specific format
- Maps to OAS `securitySchemes` with `type: http`

### RFC 6750 — Bearer Token Usage
- S2.1: Authorization header — `Bearer <token>`
- S2.2: Form-encoded body — `access_token` parameter
- S2.3: URI query — `access_token` parameter (NOT RECOMMENDED)
- S3: Error codes — invalid_request, invalid_token, insufficient_scope

### RFC 7519 — JSON Web Token (JWT)
- S4: Claims — iss, sub, aud, exp, nbf, iat, jti
- S4.1: Registered claims — standard claim names
- S7: Validation — issuer, audience, expiration checks
- Used with OAS `securitySchemes` type `http` scheme `bearer` bearerFormat `JWT`

### RFC 6749 — OAuth 2.0
- S1.3: Authorization grant types — code, implicit, client credentials, resource owner password
- S4.1: Authorization code flow
- S4.4: Client credentials flow
- Maps to OAS `securitySchemes` with `type: oauth2` and `flows`

## P3 — Extended

| RFC | Title | Key Sections |
|-----|-------|--------------|
| 6902 | JSON Patch | S3-4: operations |
| 7396 | JSON Merge Patch | S2: processing |
| 9535 | JSONPath | S2: syntax |
| 8288 | Web Linking | S3: link header |
| 8941 | Structured Fields | S3: types |

### RFC 6902 — JSON Patch
- S3: Document structure — array of operation objects
- S4: Operations — add, remove, replace, move, copy, test
- Content type: `application/json-patch+json`

### RFC 7396 — JSON Merge Patch
- S2: Processing — null removes key, other values replace
- Content type: `application/merge-patch+json`
- Simpler than JSON Patch but cannot set null or manipulate arrays

### RFC 9535 — JSONPath
- S2: Syntax — `$` root, `.key` child, `[0]` index, `[*]` wildcard, `..` descendant
- Alternative to JSON Pointer for path expressions
- Used in some OAS extension tooling

### RFC 8288 — Web Linking
- S3: Link header field — `<uri>; rel="next"; type="application/json"`
- S3.3: Relation types — registered and extension
- Maps to OAS Link objects

### RFC 8941 — Structured Fields
- S3.1: Lists, S3.2: Dictionaries, S3.3: Items
- S3.3.1: Integers, S3.3.2: Decimals, S3.3.3: Strings, S3.3.4: Tokens, S3.3.5: Byte sequences, S3.3.6: Booleans
- Modern HTTP header parsing format

## Non-RFC Specifications

| Spec | Version | URL | Notes |
|------|---------|-----|-------|
| OpenAPI | 3.2.0 | https://spec.openapis.org/oas/v3.2.0.html | Primary spec |
| JSON Schema | 2020-12 | https://json-schema.org/draft/2020-12/json-schema-core | Schema validation |
| YAML | 1.2 | https://yaml.org/spec/1.2.2/ | OpenAPI source format (libfyaml) |
| ECMA-262 | latest | https://tc39.es/ecma262/ | `pattern` regex semantics (S22 RegExp) |
