#include <liboas/oas_schema.h>

#include <errno.h>
#include <string.h>

oas_schema_t *oas_schema_create(oas_arena_t *arena)
{
    if (!arena) {
        return nullptr;
    }

    oas_schema_t *s = oas_arena_alloc(arena, sizeof(*s), _Alignof(oas_schema_t));
    if (!s) {
        return nullptr;
    }

    memset(s, 0, sizeof(*s));
    s->min_length = -1;
    s->max_length = -1;
    s->min_items = -1;
    s->max_items = -1;
    s->min_properties = -1;
    s->max_properties = -1;
    s->min_contains = -1;
    s->max_contains = -1;
    return s;
}

int oas_schema_add_property(oas_arena_t *arena, oas_schema_t *schema, const char *name,
                            oas_schema_t *prop_schema)
{
    if (!arena || !schema || !name || !prop_schema) {
        return -EINVAL;
    }

    oas_property_t *prop = oas_arena_alloc(arena, sizeof(*prop), _Alignof(oas_property_t));
    if (!prop) {
        return -ENOMEM;
    }

    prop->name = name;
    prop->schema = prop_schema;
    prop->next = schema->properties;
    schema->properties = prop;
    schema->properties_count++;
    return 0;
}

uint8_t oas_type_from_string(const char *type_name)
{
    if (!type_name) {
        return 0;
    }

    if (strcmp(type_name, "null") == 0) {
        return OAS_TYPE_NULL;
    }
    if (strcmp(type_name, "boolean") == 0) {
        return OAS_TYPE_BOOLEAN;
    }
    if (strcmp(type_name, "integer") == 0) {
        return OAS_TYPE_INTEGER;
    }
    if (strcmp(type_name, "number") == 0) {
        return OAS_TYPE_NUMBER;
    }
    if (strcmp(type_name, "string") == 0) {
        return OAS_TYPE_STRING;
    }
    if (strcmp(type_name, "array") == 0) {
        return OAS_TYPE_ARRAY;
    }
    if (strcmp(type_name, "object") == 0) {
        return OAS_TYPE_OBJECT;
    }
    return 0;
}
