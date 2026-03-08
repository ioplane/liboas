/**
 * @file oas_schema.h
 * @brief JSON Schema 2020-12 type representation for OpenAPI 3.2.
 *
 * Core data structure for schema validation. Supports type bitmasks,
 * string/numeric/array/object constraints, composition (allOf/oneOf/anyOf),
 * conditional (if/then/else), discriminator, and 2020-12 unevaluated keywords.
 */

#ifndef LIBOAS_OAS_SCHEMA_H
#define LIBOAS_OAS_SCHEMA_H

#include <liboas/oas_alloc.h>
#include <liboas/oas_error.h>

#include <yyjson.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** JSON Schema type bitmask values */
typedef enum : uint8_t {
    OAS_TYPE_NULL = 0x01,
    OAS_TYPE_BOOLEAN = 0x02,
    OAS_TYPE_INTEGER = 0x04,
    OAS_TYPE_NUMBER = 0x08,
    OAS_TYPE_STRING = 0x10,
    OAS_TYPE_ARRAY = 0x20,
    OAS_TYPE_OBJECT = 0x40,
} oas_type_t;

typedef struct oas_schema oas_schema_t;
typedef struct oas_property oas_property_t;
typedef struct oas_discriminator_mapping oas_discriminator_mapping_t;

struct oas_discriminator_mapping {
    const char *key; /**< mapping key */
    const char *ref; /**< $ref to schema */
};

struct oas_property {
    const char *name;
    oas_schema_t *schema;
    struct oas_property *next;
};

struct oas_schema {
    uint8_t type_mask; /**< bitmask of OAS_TYPE_* (supports type arrays) */
    const char *title;
    const char *description;
    const char *format; /**< date, date-time, email, uri, uuid, etc. */

    /* String constraints */
    int64_t min_length;  /**< -1 = not set */
    int64_t max_length;  /**< -1 = not set */
    const char *pattern; /**< ECMA-262 regex (backend-agnostic) */

    /* Numeric constraints */
    double minimum;
    double maximum;
    double exclusive_minimum;
    double exclusive_maximum;
    double multiple_of;
    bool has_minimum;
    bool has_maximum;
    bool has_exclusive_minimum;
    bool has_exclusive_maximum;
    bool has_multiple_of;

    /* Array constraints */
    oas_schema_t *items;         /**< items schema */
    oas_schema_t **prefix_items; /**< tuple validation (2020-12) */
    size_t prefix_items_count;
    int64_t min_items; /**< -1 = not set */
    int64_t max_items; /**< -1 = not set */
    bool unique_items;

    /* Object constraints */
    oas_property_t *properties; /**< linked list */
    size_t properties_count;
    const char **required; /**< array of required property names */
    size_t required_count;
    oas_schema_t *additional_properties; /**< nullptr = no constraint */
    bool additional_properties_bool;     /**< true if additionalProperties is boolean */

    /* Composition */
    oas_schema_t **all_of;
    size_t all_of_count;
    oas_schema_t **any_of;
    size_t any_of_count;
    oas_schema_t **one_of;
    size_t one_of_count;
    oas_schema_t *not_schema;

    /* Conditional */
    oas_schema_t *if_schema;
    oas_schema_t *then_schema;
    oas_schema_t *else_schema;

    /* Const/Enum */
    yyjson_val *const_value; /**< zero-copy from yyjson doc */
    yyjson_val *enum_values; /**< array of allowed values */

    /* $ref */
    const char *ref;            /**< raw $ref string */
    oas_schema_t *ref_resolved; /**< resolved target (nullptr if unresolved) */

    /* Default */
    yyjson_val *default_value;

    /* Nullable (OAS 3.0 compat: nullable -> type includes null) */
    bool nullable;

    /* Read/Write only */
    bool read_only;
    bool write_only;

    /* Discriminator (OpenAPI 3.2 polymorphism) */
    const char *discriminator_property;                 /**< propertyName */
    oas_discriminator_mapping_t *discriminator_mapping; /**< mapping entries */
    size_t discriminator_mapping_count;

    /* 2020-12 advanced */
    bool has_unevaluated_properties;
    oas_schema_t *unevaluated_properties;
    bool has_unevaluated_items;
    oas_schema_t *unevaluated_items;
};

/**
 * @brief Allocate and zero-initialize a schema node.
 * @param arena Arena allocator.
 * @return Schema with sensible defaults, or nullptr on failure.
 */
[[nodiscard]] oas_schema_t *oas_schema_create(oas_arena_t *arena);

/**
 * @brief Add a property to a schema's property list.
 * @param arena  Arena allocator.
 * @param schema Target schema (must be object-like).
 * @param name   Property name.
 * @param prop_schema Property's schema.
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] int oas_schema_add_property(oas_arena_t *arena, oas_schema_t *schema,
                                          const char *name, oas_schema_t *prop_schema);

/**
 * @brief Convert a JSON type name string to type bitmask.
 * @param type_name Type name ("string", "integer", "number", etc.).
 * @return Bitmask value, or 0 if unknown.
 */
uint8_t oas_type_from_string(const char *type_name);

#endif /* LIBOAS_OAS_SCHEMA_H */
