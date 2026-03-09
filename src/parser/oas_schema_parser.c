#include "oas_schema_parser.h"

#include <liboas/oas_schema.h>

#include <yyjson.h>

#include <errno.h>
#include <string.h>

/* yyjson_get_real returns 0 for integer values; this handles both */
static double get_number(yyjson_val *v)
{
    if (yyjson_is_real(v)) {
        return yyjson_get_real(v);
    }
    if (yyjson_is_int(v)) {
        return (double)yyjson_get_sint(v);
    }
    return 0.0;
}

static void parse_type(oas_schema_t *schema, yyjson_val *val)
{
    if (yyjson_is_str(val)) {
        schema->type_mask = oas_type_from_string(yyjson_get_str(val));
    } else if (yyjson_is_arr(val)) {
        /* type: ["string", "null"] */
        yyjson_val *item;
        size_t idx;
        size_t max;
        yyjson_arr_foreach(val, idx, max, item)
        {
            if (yyjson_is_str(item)) {
                schema->type_mask |= oas_type_from_string(yyjson_get_str(item));
            }
        }
    }
}

static void parse_string_constraints(oas_schema_t *schema, yyjson_val *obj)
{
    yyjson_val *v;

    v = yyjson_obj_get(obj, "minLength");
    if (v && yyjson_is_int(v)) {
        schema->min_length = yyjson_get_sint(v);
    }

    v = yyjson_obj_get(obj, "maxLength");
    if (v && yyjson_is_int(v)) {
        schema->max_length = yyjson_get_sint(v);
    }

    v = yyjson_obj_get(obj, "pattern");
    if (v && yyjson_is_str(v)) {
        schema->pattern = yyjson_get_str(v);
    }
}

static void parse_numeric_constraints(oas_schema_t *schema, yyjson_val *obj)
{
    yyjson_val *v;

    v = yyjson_obj_get(obj, "minimum");
    if (v && yyjson_is_num(v)) {
        schema->minimum = get_number(v);
        schema->has_minimum = true;
    }

    v = yyjson_obj_get(obj, "maximum");
    if (v && yyjson_is_num(v)) {
        schema->maximum = get_number(v);
        schema->has_maximum = true;
    }

    v = yyjson_obj_get(obj, "exclusiveMinimum");
    if (v && yyjson_is_num(v)) {
        schema->exclusive_minimum = get_number(v);
        schema->has_exclusive_minimum = true;
    }

    v = yyjson_obj_get(obj, "exclusiveMaximum");
    if (v && yyjson_is_num(v)) {
        schema->exclusive_maximum = get_number(v);
        schema->has_exclusive_maximum = true;
    }

    v = yyjson_obj_get(obj, "multipleOf");
    if (v && yyjson_is_num(v)) {
        schema->multiple_of = get_number(v);
        schema->has_multiple_of = true;
    }
}

static void parse_array_constraints(oas_schema_t *schema, yyjson_val *obj, oas_arena_t *arena,
                                    oas_error_list_t *errors)
{
    yyjson_val *v;

    v = yyjson_obj_get(obj, "items");
    if (v && yyjson_is_obj(v)) {
        schema->items = oas_schema_parse(arena, v, errors);
    }

    v = yyjson_obj_get(obj, "prefixItems");
    if (v && yyjson_is_arr(v)) {
        size_t count = yyjson_arr_size(v);
        if (count > 0) {
            schema->prefix_items =
                oas_arena_alloc(arena, sizeof(oas_schema_t *) * count, _Alignof(oas_schema_t *));
            if (schema->prefix_items) {
                schema->prefix_items_count = count;
                yyjson_val *item;
                size_t idx;
                size_t max;
                yyjson_arr_foreach(v, idx, max, item)
                {
                    schema->prefix_items[idx] = oas_schema_parse(arena, item, errors);
                }
            }
        }
    }

    v = yyjson_obj_get(obj, "minItems");
    if (v && yyjson_is_int(v)) {
        schema->min_items = yyjson_get_sint(v);
    }

    v = yyjson_obj_get(obj, "maxItems");
    if (v && yyjson_is_int(v)) {
        schema->max_items = yyjson_get_sint(v);
    }

    v = yyjson_obj_get(obj, "uniqueItems");
    if (v && yyjson_is_bool(v)) {
        schema->unique_items = yyjson_get_bool(v);
    }
}

static void parse_object_constraints(oas_schema_t *schema, yyjson_val *obj, oas_arena_t *arena,
                                     oas_error_list_t *errors)
{
    yyjson_val *v;

    v = yyjson_obj_get(obj, "properties");
    if (v && yyjson_is_obj(v)) {
        yyjson_val *key;
        yyjson_val *val;
        size_t idx;
        size_t max;
        yyjson_obj_foreach(v, idx, max, key, val)
        {
            if (yyjson_is_str(key) && yyjson_is_obj(val)) {
                oas_schema_t *prop_schema = oas_schema_parse(arena, val, errors);
                if (prop_schema) {
                    (void)oas_schema_add_property(arena, schema, yyjson_get_str(key), prop_schema);
                }
            }
        }
    }

    v = yyjson_obj_get(obj, "required");
    if (v && yyjson_is_arr(v)) {
        size_t count = yyjson_arr_size(v);
        if (count > 0) {
            schema->required =
                oas_arena_alloc(arena, sizeof(const char *) * count, _Alignof(const char *));
            if (schema->required) {
                schema->required_count = count;
                yyjson_val *item;
                size_t idx2;
                size_t max2;
                yyjson_arr_foreach(v, idx2, max2, item)
                {
                    if (yyjson_is_str(item)) {
                        schema->required[idx2] = yyjson_get_str(item);
                    }
                }
            }
        }
    }

    v = yyjson_obj_get(obj, "additionalProperties");
    if (v) {
        if (yyjson_is_bool(v)) {
            schema->additional_properties_bool = true;
            /* For boolean false, we still need a sentinel schema */
            if (!yyjson_get_bool(v)) {
                schema->additional_properties = oas_schema_create(arena);
            }
        } else if (yyjson_is_obj(v)) {
            schema->additional_properties = oas_schema_parse(arena, v, errors);
        }
    }
}

static oas_schema_t **parse_schema_array(oas_arena_t *arena, yyjson_val *arr, size_t *out_count,
                                         oas_error_list_t *errors)
{
    size_t count = yyjson_arr_size(arr);
    if (count == 0) {
        *out_count = 0;
        return nullptr;
    }

    oas_schema_t **schemas =
        oas_arena_alloc(arena, sizeof(oas_schema_t *) * count, _Alignof(oas_schema_t *));
    if (!schemas) {
        *out_count = 0;
        return nullptr;
    }

    yyjson_val *item;
    size_t idx;
    size_t max;
    yyjson_arr_foreach(arr, idx, max, item)
    {
        schemas[idx] = oas_schema_parse(arena, item, errors);
    }
    *out_count = count;
    return schemas;
}

static void parse_composition(oas_schema_t *schema, yyjson_val *obj, oas_arena_t *arena,
                              oas_error_list_t *errors)
{
    yyjson_val *v;

    v = yyjson_obj_get(obj, "allOf");
    if (v && yyjson_is_arr(v)) {
        schema->all_of = parse_schema_array(arena, v, &schema->all_of_count, errors);
    }

    v = yyjson_obj_get(obj, "anyOf");
    if (v && yyjson_is_arr(v)) {
        schema->any_of = parse_schema_array(arena, v, &schema->any_of_count, errors);
    }

    v = yyjson_obj_get(obj, "oneOf");
    if (v && yyjson_is_arr(v)) {
        schema->one_of = parse_schema_array(arena, v, &schema->one_of_count, errors);
    }

    v = yyjson_obj_get(obj, "not");
    if (v && yyjson_is_obj(v)) {
        schema->not_schema = oas_schema_parse(arena, v, errors);
    }
}

static void parse_conditional(oas_schema_t *schema, yyjson_val *obj, oas_arena_t *arena,
                              oas_error_list_t *errors)
{
    yyjson_val *v;

    v = yyjson_obj_get(obj, "if");
    if (v && yyjson_is_obj(v)) {
        schema->if_schema = oas_schema_parse(arena, v, errors);
    }

    v = yyjson_obj_get(obj, "then");
    if (v && yyjson_is_obj(v)) {
        schema->then_schema = oas_schema_parse(arena, v, errors);
    }

    v = yyjson_obj_get(obj, "else");
    if (v && yyjson_is_obj(v)) {
        schema->else_schema = oas_schema_parse(arena, v, errors);
    }
}

oas_schema_t *oas_schema_parse(oas_arena_t *arena, yyjson_val *val, oas_error_list_t *errors)
{
    if (!arena || !val || !yyjson_is_obj(val)) {
        return nullptr;
    }

    oas_schema_t *schema = oas_schema_create(arena);
    if (!schema) {
        return nullptr;
    }

    /* $ref — store reference URI; in OAS 3.1+/JSON Schema 2020-12, siblings are valid */
    yyjson_val *ref = yyjson_obj_get(val, "$ref");
    if (ref && yyjson_is_str(ref)) {
        schema->ref = yyjson_get_str(ref);
    }

    /* type */
    yyjson_val *type_val = yyjson_obj_get(val, "type");
    if (type_val) {
        parse_type(schema, type_val);
    }

    /* Metadata */
    yyjson_val *v;
    v = yyjson_obj_get(val, "title");
    if (v && yyjson_is_str(v)) {
        schema->title = yyjson_get_str(v);
    }
    v = yyjson_obj_get(val, "description");
    if (v && yyjson_is_str(v)) {
        schema->description = yyjson_get_str(v);
    }
    v = yyjson_obj_get(val, "format");
    if (v && yyjson_is_str(v)) {
        schema->format = yyjson_get_str(v);
    }

    /* Constraints */
    parse_string_constraints(schema, val);
    parse_numeric_constraints(schema, val);
    parse_array_constraints(schema, val, arena, errors);
    parse_object_constraints(schema, val, arena, errors);

    /* Composition & conditional */
    parse_composition(schema, val, arena, errors);
    parse_conditional(schema, val, arena, errors);

    /* Const/enum */
    v = yyjson_obj_get(val, "const");
    if (v) {
        schema->const_value = v;
    }
    v = yyjson_obj_get(val, "enum");
    if (v && yyjson_is_arr(v)) {
        schema->enum_values = v;
    }

    /* Default */
    v = yyjson_obj_get(val, "default");
    if (v) {
        schema->default_value = v;
    }

    /* nullable (OAS 3.0 compat) */
    v = yyjson_obj_get(val, "nullable");
    if (v && yyjson_is_bool(v) && yyjson_get_bool(v)) {
        schema->nullable = true;
        schema->type_mask |= OAS_TYPE_NULL;
    }

    /* readOnly / writeOnly */
    v = yyjson_obj_get(val, "readOnly");
    if (v && yyjson_is_bool(v)) {
        schema->read_only = yyjson_get_bool(v);
    }
    v = yyjson_obj_get(val, "writeOnly");
    if (v && yyjson_is_bool(v)) {
        schema->write_only = yyjson_get_bool(v);
    }

    /* Discriminator */
    v = yyjson_obj_get(val, "discriminator");
    if (v && yyjson_is_obj(v)) {
        yyjson_val *prop_name = yyjson_obj_get(v, "propertyName");
        if (prop_name && yyjson_is_str(prop_name)) {
            schema->discriminator_property = yyjson_get_str(prop_name);
        }
        yyjson_val *mapping = yyjson_obj_get(v, "mapping");
        if (mapping && yyjson_is_obj(mapping)) {
            size_t count = yyjson_obj_size(mapping);
            if (count > 0) {
                schema->discriminator_mapping =
                    oas_arena_alloc(arena, sizeof(oas_discriminator_mapping_t) * count,
                                    _Alignof(oas_discriminator_mapping_t));
                if (schema->discriminator_mapping) {
                    schema->discriminator_mapping_count = count;
                    yyjson_val *mk;
                    yyjson_val *mv;
                    size_t mi;
                    size_t mm;
                    yyjson_obj_foreach(mapping, mi, mm, mk, mv)
                    {
                        schema->discriminator_mapping[mi].key = yyjson_get_str(mk);
                        schema->discriminator_mapping[mi].ref =
                            yyjson_is_str(mv) ? yyjson_get_str(mv) : nullptr;
                    }
                }
            }
        }
    }

    /* 2020-12: unevaluatedProperties / unevaluatedItems */
    v = yyjson_obj_get(val, "unevaluatedProperties");
    if (v) {
        schema->has_unevaluated_properties = true;
        if (yyjson_is_obj(v)) {
            schema->unevaluated_properties = oas_schema_parse(arena, v, errors);
        }
    }
    v = yyjson_obj_get(val, "unevaluatedItems");
    if (v) {
        schema->has_unevaluated_items = true;
        if (yyjson_is_obj(v)) {
            schema->unevaluated_items = oas_schema_parse(arena, v, errors);
        }
    }

    return schema;
}
