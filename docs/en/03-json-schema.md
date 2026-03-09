# JSON Schema 2020-12

liboas implements JSON Schema 2020-12 validation as required by OpenAPI 3.2.
The schema representation is defined in `oas_schema.h`.

## oas_schema_t Structure

`oas_schema_t` is a flat struct containing all possible JSON Schema keywords.
Unused fields use sentinel values (`-1` for optional integers, `nullptr` for
pointers, `false` for booleans). The struct is arena-allocated via
`oas_schema_create()`.

## Type Bitmask System

Types are represented as a bitmask in `type_mask`, allowing schemas to accept
multiple types (e.g. `["string", "null"]`).

| Constant          | Value  | JSON Schema type |
|-------------------|--------|------------------|
| `OAS_TYPE_NULL`   | `0x01` | `null`           |
| `OAS_TYPE_BOOLEAN`| `0x02` | `boolean`        |
| `OAS_TYPE_INTEGER`| `0x04` | `integer`        |
| `OAS_TYPE_NUMBER` | `0x08` | `number`         |
| `OAS_TYPE_STRING` | `0x10` | `string`         |
| `OAS_TYPE_ARRAY`  | `0x20` | `array`          |
| `OAS_TYPE_OBJECT` | `0x40` | `object`         |

A `type_mask` of `0x00` means no type constraint (any type accepted). Use
`oas_type_from_string()` to convert a type name to its bitmask value.

Per JSON Schema, `integer` is a subset of `number`. When `OAS_TYPE_NUMBER` is
set, integer values also pass validation.

## Constraints by Type

### String Constraints

| Field        | Type          | Description                              |
|--------------|---------------|------------------------------------------|
| `min_length` | `int64_t`     | Minimum string length (-1 = not set)     |
| `max_length` | `int64_t`     | Maximum string length (-1 = not set)     |
| `pattern`    | `const char*` | ECMA-262 regex (validated by regex backend) |
| `format`     | `const char*` | Format hint (e.g. `"email"`, `"uuid"`)   |

Pattern matching uses the regex backend vtable, defaulting to QuickJS libregexp
for ECMA-262 compliance. Patterns are unanchored per JSON Schema specification.

### Numeric Constraints

| Field                | Type     | Description                        |
|----------------------|----------|------------------------------------|
| `minimum`            | `double` | Minimum value (inclusive)          |
| `maximum`            | `double` | Maximum value (inclusive)          |
| `exclusive_minimum`  | `double` | Exclusive minimum                  |
| `exclusive_maximum`  | `double` | Exclusive maximum                  |
| `multiple_of`        | `double` | Value must be divisible by this    |

Each numeric field has a corresponding `has_*` boolean flag since 0.0 is a
valid constraint value that cannot serve as a sentinel.

### Array Constraints

| Field               | Type             | Description                         |
|---------------------|------------------|-------------------------------------|
| `items`             | `oas_schema_t *` | Schema for all array elements       |
| `prefix_items`      | `oas_schema_t **`| Tuple validation (positional schemas)|
| `prefix_items_count`| `size_t`         | Number of prefix item schemas       |
| `min_items`         | `int64_t`        | Minimum array length (-1 = not set) |
| `max_items`         | `int64_t`        | Maximum array length (-1 = not set) |
| `unique_items`      | `bool`           | All elements must be distinct       |
| `contains`          | `oas_schema_t *` | At least one element must match     |
| `min_contains`      | `int64_t`        | Minimum matching elements for contains |
| `max_contains`      | `int64_t`        | Maximum matching elements for contains |

When `prefix_items` is set, elements at each index are validated against the
corresponding schema. Elements beyond the prefix are validated against `items`.

### Object Constraints

| Field                    | Type                        | Description                       |
|--------------------------|-----------------------------|-----------------------------------|
| `properties`             | `oas_property_t *`          | Linked list of named properties   |
| `properties_count`       | `size_t`                    | Number of properties              |
| `required`               | `const char **`             | Required property names           |
| `required_count`         | `size_t`                    | Number of required properties     |
| `additional_properties`  | `oas_schema_t *`            | Schema for extra properties       |
| `additional_properties_bool` | `bool`                  | True if set as boolean false      |
| `min_properties`         | `int64_t`                   | Minimum property count            |
| `max_properties`         | `int64_t`                   | Maximum property count            |
| `property_names`         | `oas_schema_t *`            | Schema all property names must match |
| `pattern_properties`     | `oas_pattern_property_t *`  | Regex-keyed property schemas      |
| `dependent_required`     | `oas_dependent_required_t *`| Conditional required properties   |
| `dependent_schemas`      | `oas_dependent_schema_t *`  | Conditional schema application    |

Properties are stored as a linked list (`oas_property_t`). Each node has a
`name`, `schema`, and `next` pointer. Use `oas_schema_add_property()` to
append.

## Composition Keywords

| Field       | Type             | JSON Schema keyword |
|-------------|------------------|---------------------|
| `all_of`    | `oas_schema_t **`| `allOf` -- all must match |
| `any_of`    | `oas_schema_t **`| `anyOf` -- at least one must match |
| `one_of`    | `oas_schema_t **`| `oneOf` -- exactly one must match |
| `not_schema`| `oas_schema_t *` | `not` -- must not match |

Each composition array has a corresponding `*_count` field.

## Conditional Keywords

| Field         | Type             | JSON Schema keyword |
|---------------|------------------|---------------------|
| `if_schema`   | `oas_schema_t *` | `if`                |
| `then_schema` | `oas_schema_t *` | `then`              |
| `else_schema` | `oas_schema_t *` | `else`              |

When `if` validates successfully, `then` is applied. Otherwise, `else` is
applied. `then` and `else` have no effect without `if`.

## Discriminator

OpenAPI polymorphism via discriminator:

- `discriminator_property` -- property name used to select the schema
- `discriminator_mapping` -- array of key-to-`$ref` mappings
- `discriminator_mapping_count` -- number of mapping entries

The discriminator property value selects which `oneOf`/`anyOf` subschema to
validate against, avoiding the need to try all candidates.

## Const and Enum

- `const_value` (`yyjson_val *`) -- value must be exactly equal (deep compare).
- `enum_values` (`yyjson_val *`) -- value must be one of the listed values.

Both reference zero-copy yyjson values from the parsed document.

## $ref Resolution

- `ref` -- the raw `$ref` string (e.g. `"#/components/schemas/Pet"`).
- `ref_resolved` -- pointer to the resolved target schema after resolution.

When `ref_resolved` is set, the validator follows it transparently. The
original `ref` string is preserved for diagnostic and emission purposes.

## 2020-12 Advanced Keywords

| Field                       | Type             | Description                        |
|-----------------------------|------------------|------------------------------------|
| `unevaluated_properties`    | `oas_schema_t *` | Schema for properties not covered by `properties`, `patternProperties`, or composition |
| `unevaluated_items`         | `oas_schema_t *` | Schema for array items not covered by `prefixItems` or `items` |

These require tracking which properties/items were "evaluated" during
validation of composition keywords.

## Read/Write Context

- `read_only` -- property appears only in responses.
- `write_only` -- property appears only in requests.
- `deprecated` -- marks the schema as deprecated.
- `nullable` -- OAS 3.0 compatibility flag; internally adds `OAS_TYPE_NULL` to `type_mask`.
