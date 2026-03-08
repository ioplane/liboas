# RFC Reference

Local RFC text copies for offline reference during liboas development.

## Download RFCs

```bash
python3 scripts/rfc-scraper.py --download docs/rfc/
```

## Generate Index

```bash
python3 scripts/rfc-scraper.py -o docs/rfc/INDEX.md
```

## Priority

| Priority | RFCs | Domain |
|----------|------|--------|
| P0 — Core | 8259, 6901, 3986, 6570, 9110 | JSON, JSON Pointer, URI, HTTP Semantics |
| P1 — Validation | 9457, 7578, 2045, 2046, 4648, 6838 | Problem Details, multipart, MIME, encoding |
| P2 — Security | 6749, 6750, 7519, 7515, 7517, 7235, 7617, 7636, 8414 | OAuth, JWT, JWS, JWK, HTTP Auth |
| P3 — Extended | 6902, 7396, 9535, 8288, 8941 | JSON Patch, JSONPath, Web Linking |

## JSON Core

| RFC | Title |
|-----|-------|
| 8259 | The JavaScript Object Notation (JSON) Data Interchange Format |
| 7493 | The I-JSON Message Format |

## JSON Pointer & Patch

| RFC | Title |
|-----|-------|
| 6901 | JavaScript Object Notation (JSON) Pointer |
| 6902 | JavaScript Object Notation (JSON) Patch |
| 7396 | JSON Merge Patch |
| 9535 | JSONPath: Query Expressions for JSON |

## URI & Templates

| RFC | Title |
|-----|-------|
| 3986 | Uniform Resource Identifier (URI): Generic Syntax |
| 6570 | URI Template |
| 3987 | Internationalized Resource Identifiers (IRIs) |

## HTTP Semantics

| RFC | Title |
|-----|-------|
| 9110 | HTTP Semantics |
| 9111 | HTTP Caching |
| 9457 | Problem Details for HTTP APIs |
| 7235 | HTTP/1.1: Authentication |
| 7617 | The 'Basic' HTTP Authentication Scheme |
| 6585 | Additional HTTP Status Codes |

## Media Types & Encoding

| RFC | Title |
|-----|-------|
| 2045 | MIME Part One: Format of Internet Message Bodies |
| 2046 | MIME Part Two: Media Types |
| 7578 | Returning Values from Forms: multipart/form-data |
| 4648 | The Base16, Base32, and Base64 Data Encodings |
| 6838 | Media Type Specifications and Registration Procedures |

## Authentication (JWT/OAuth)

| RFC | Title |
|-----|-------|
| 6749 | The OAuth 2.0 Authorization Framework |
| 6750 | OAuth 2.0 Bearer Token Usage |
| 7519 | JSON Web Token (JWT) |
| 7515 | JSON Web Signature (JWS) |
| 7516 | JSON Web Encryption (JWE) |
| 7517 | JSON Web Key (JWK) |
| 7636 | Proof Key for Code Exchange (PKCE) |
| 8414 | OAuth 2.0 Authorization Server Metadata |

## Web APIs

| RFC | Title |
|-----|-------|
| 8288 | Web Linking |
| 8941 | Structured Field Values for HTTP |
| 6265 | HTTP State Management Mechanism (Cookies) |
| 6797 | HTTP Strict Transport Security (HSTS) |
