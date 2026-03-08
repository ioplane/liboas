---
name: json-schema-patterns
description: Use when implementing JSON Schema 2020-12 validation — type system, composition keywords, format validators, $ref resolution, vocabulary system. MANDATORY for src/compiler/ and src/validator/.
---

# JSON Schema 2020-12 Patterns

## Type System

- **Primitive types**: null, boolean, integer, number, string, array, object
- `type` can be string or array: `"string"` or `["string", "null"]`
- Type coercion: integer is a subset of number (1.0 validates as integer)

## Validation Keywords by Type

### string
- `minLength`, `maxLength`: Unicode codepoint count (not byte length)
- `pattern`: ECMA-262 regex, **unanchored** match (via `oas_regex_backend_t` vtable: PCRE2 default or libregexp strict)
- `format`: date, date-time, time, email, uri, uri-reference, uuid, ipv4, ipv6, hostname

### number / integer
- `minimum`, `maximum`: inclusive bounds
- `exclusiveMinimum`, `exclusiveMaximum`: exclusive bounds (numeric in 2020-12, not boolean)
- `multipleOf`: exact division check (use epsilon for floating point)

### array
- `items`: schema applied to all items (or items after prefixItems)
- `prefixItems`: tuple validation — array of schemas for positional items
- `minItems`, `maxItems`: length constraints
- `uniqueItems`: all elements must be distinct (deep equality)
- `contains`: at least one item must match
- `minContains`, `maxContains`: constrain how many items match `contains`

### object
- `properties`: map of property name to schema
- `patternProperties`: map of regex pattern to schema
- `additionalProperties`: schema for properties not matched by `properties` or `patternProperties`
- `required`: array of required property names
- `minProperties`, `maxProperties`: property count constraints
- `propertyNames`: schema that all property names must match
- `dependentRequired`: if property A present, properties B/C must also be present
- `dependentSchemas`: if property A present, apply additional schema

### any type
- `const`: exact value match (deep equality)
- `enum`: value must be one of the listed values
- `allOf`: must match ALL schemas (intersection)
- `anyOf`: must match AT LEAST ONE schema (union)
- `oneOf`: must match EXACTLY ONE schema (exclusive)
- `not`: must NOT match the schema
- `if`/`then`/`else`: conditional schema application

## Advanced Keywords (2020-12 specific)

- `$dynamicAnchor` / `$dynamicRef`: Recursive schema extension — dynamic scope resolution enables extensible recursive schemas (e.g., extending a base tree schema with additional node types)
- `unevaluatedProperties` / `unevaluatedItems`: "catch-all" after composition — applies only to properties/items not evaluated by any subschema in allOf/oneOf/anyOf/if-then-else
- `$vocabulary`: Declare supported keyword sets — meta-schema declares which vocabularies are required vs optional
- `$id` / `$anchor`: Schema identification — `$id` sets base URI, `$anchor` creates plain-name fragment

## $ref Resolution Algorithm

1. Parse `$ref` URI using RFC 3986
2. If fragment-only (`#/definitions/Foo`): resolve via JSON Pointer (RFC 6901)
3. If relative URI: resolve against base URI (`$id` of enclosing schema)
4. If absolute URI: fetch remote document (with caching and timeout)
5. Cycle detection: mark visited nodes, return error on cycle
6. After resolution: `$ref` sibling keywords ARE valid in 2020-12 (unlike draft-07 where `$ref` consumed all siblings)

## Compilation Strategy

- Walk schema tree depth-first, emit flat instruction array
- **Instructions**: CHECK_TYPE, CHECK_MIN, CHECK_MAX, CHECK_PATTERN, CHECK_FORMAT, CHECK_REQUIRED, CHECK_ENUM, CHECK_CONST, ENTER_OBJECT, ENTER_ARRAY, ENTER_PROPERTY, BRANCH_ALLOF, BRANCH_ONEOF, BRANCH_ANYOF, NEGATE, COND_IF, COND_THEN, COND_ELSE, END
- Pre-compile regex patterns via `oas_regex_backend_t` vtable (PCRE2 JIT default, libregexp optional) at compile time
- Constant-fold: `{"type": "string", "minLength": 0}` becomes just CHECK_TYPE
- Inline small `$ref` targets into instruction stream, keep pointer for large ones
- Track "evaluated" properties/items for unevaluatedProperties/unevaluatedItems support

## Format Validators (built-in)

| Format | Spec | Notes |
|--------|------|-------|
| date | ISO 8601 / RFC 3339 full-date | YYYY-MM-DD, validate leap years |
| date-time | RFC 3339 | Full date-time with mandatory offset |
| time | RFC 3339 full-time | HH:MM:SS with optional fractional seconds and offset |
| email | RFC 5321 addr-spec | Mailbox format, not display name |
| uri | RFC 3986 | Absolute URI with scheme |
| uri-reference | RFC 3986 | URI or relative-reference |
| uuid | RFC 4122 | 8-4-4-4-12 hex format |
| ipv4 | RFC 791 | Dotted decimal, no leading zeros |
| ipv6 | RFC 4291 | Full and compressed forms |
| hostname | RFC 1123 | Labels, max 253 chars total |

Format validation is OPTIONAL per the JSON Schema spec — enforce via configurable policy (`OAS_FORMAT_IGNORE`, `OAS_FORMAT_WARN`, `OAS_FORMAT_ENFORCE`).

## Discriminator (OpenAPI 3.2)

- `discriminator.propertyName`: property whose value selects the schema variant
- `discriminator.mapping`: map of value → `$ref` (optional, auto-maps to schema name if absent)
- Used with `oneOf` / `anyOf` for polymorphic dispatch — avoids brute-force schema testing
- Compiler should emit `OAS_OP_DISCRIMINATOR` that reads property value and jumps directly to matching sub-schema
- Validation: if discriminator present, use mapping instead of trying all branches

## Anti-Patterns

- **NEVER** validate format by default (DoS risk with pathological regex input)
- **NEVER** recurse without depth limit (`$ref` chains can be deeply nested or circular)
- **NEVER** allocate per-validation (use pre-compiled schemas; arena for error collection)
- **NEVER** ignore `unevaluatedProperties` (breaks allOf/oneOf composition semantics)
- **NEVER** treat integer as different from number in numeric comparisons (1 == 1.0)
- **NEVER** assume `pattern` is anchored — JSON Schema patterns are unanchored by default
- **NEVER** count string length in bytes — use Unicode codepoints for minLength/maxLength
