#include <liboas/oas_builder.h>

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

/* ── Primitive schema builders ─────────────────────────────────────────── */

oas_schema_t *oas_schema_build_string(oas_arena_t *arena)
{
    oas_schema_t *s = oas_schema_create(arena);
    if (!s) {
        return nullptr;
    }
    s->type_mask = OAS_TYPE_STRING;
    return s;
}

oas_schema_t *oas_schema_build_int32(oas_arena_t *arena)
{
    oas_schema_t *s = oas_schema_create(arena);
    if (!s) {
        return nullptr;
    }
    s->type_mask = OAS_TYPE_INTEGER;
    s->format = "int32";
    return s;
}

oas_schema_t *oas_schema_build_int64(oas_arena_t *arena)
{
    oas_schema_t *s = oas_schema_create(arena);
    if (!s) {
        return nullptr;
    }
    s->type_mask = OAS_TYPE_INTEGER;
    s->format = "int64";
    return s;
}

oas_schema_t *oas_schema_build_number(oas_arena_t *arena)
{
    oas_schema_t *s = oas_schema_create(arena);
    if (!s) {
        return nullptr;
    }
    s->type_mask = OAS_TYPE_NUMBER;
    return s;
}

oas_schema_t *oas_schema_build_bool(oas_arena_t *arena)
{
    oas_schema_t *s = oas_schema_create(arena);
    if (!s) {
        return nullptr;
    }
    s->type_mask = OAS_TYPE_BOOLEAN;
    return s;
}

/* ── Constrained schema builders ───────────────────────────────────────── */

oas_schema_t *oas_schema_build_string_ex(oas_arena_t *arena, const oas_string_opts_t *opts)
{
    if (!opts) {
        return oas_schema_build_string(arena);
    }

    oas_schema_t *s = oas_schema_create(arena);
    if (!s) {
        return nullptr;
    }
    s->type_mask = OAS_TYPE_STRING;
    s->min_length = opts->min_length;
    s->max_length = opts->max_length;
    s->pattern = opts->pattern;
    s->format = opts->format;
    s->description = opts->description;
    return s;
}

static oas_schema_t *build_numeric_ex(oas_arena_t *arena, uint8_t type_mask,
                                      const oas_number_opts_t *opts)
{
    oas_schema_t *s = oas_schema_create(arena);
    if (!s) {
        return nullptr;
    }
    s->type_mask = type_mask;

    if (!isnan(opts->minimum)) {
        if (opts->exclusive_min) {
            s->exclusive_minimum = opts->minimum;
            s->has_exclusive_minimum = true;
        } else {
            s->minimum = opts->minimum;
            s->has_minimum = true;
        }
    }

    if (!isnan(opts->maximum)) {
        if (opts->exclusive_max) {
            s->exclusive_maximum = opts->maximum;
            s->has_exclusive_maximum = true;
        } else {
            s->maximum = opts->maximum;
            s->has_maximum = true;
        }
    }

    if (!isnan(opts->multiple_of)) {
        s->multiple_of = opts->multiple_of;
        s->has_multiple_of = true;
    }

    s->format = opts->format;
    s->description = opts->description;
    return s;
}

oas_schema_t *oas_schema_build_integer_ex(oas_arena_t *arena, const oas_number_opts_t *opts)
{
    if (!opts) {
        return oas_schema_build_int64(arena);
    }
    return build_numeric_ex(arena, OAS_TYPE_INTEGER, opts);
}

oas_schema_t *oas_schema_build_number_ex(oas_arena_t *arena, const oas_number_opts_t *opts)
{
    if (!opts) {
        return oas_schema_build_number(arena);
    }
    return build_numeric_ex(arena, OAS_TYPE_NUMBER, opts);
}

/* ── Composite schema builders ─────────────────────────────────────────── */

oas_schema_t *oas_schema_build_array(oas_arena_t *arena, oas_schema_t *items)
{
    oas_schema_t *s = oas_schema_create(arena);
    if (!s) {
        return nullptr;
    }
    s->type_mask = OAS_TYPE_ARRAY;
    s->items = items;
    return s;
}

oas_schema_t *oas_schema_build_object(oas_arena_t *arena)
{
    oas_schema_t *s = oas_schema_create(arena);
    if (!s) {
        return nullptr;
    }
    s->type_mask = OAS_TYPE_OBJECT;
    return s;
}

/* ── Schema modifiers ──────────────────────────────────────────────────── */

int oas_schema_set_required(oas_arena_t *arena, oas_schema_t *schema, ...)
{
    if (!arena || !schema) {
        return -EINVAL;
    }

    /* First pass: count names */
    va_list ap;
    va_start(ap, schema);
    size_t count = 0;
    // codechecker_suppress [security.VAList] va_list is initialized by va_start above
    while (va_arg(ap, const char *) != nullptr) { //-V1044
        count++;
    }
    va_end(ap);

    if (count == 0) {
        return 0;
    }

    const char **names = oas_arena_alloc(arena, count * sizeof(*names), _Alignof(const char *));
    if (!names) {
        return -ENOMEM;
    }

    /* Second pass: collect names */
    va_start(ap, schema);
    for (size_t i = 0; i < count; i++) {
        names[i] = va_arg(ap, const char *);
    }
    va_end(ap);

    schema->required = names;
    schema->required_count = count;
    return 0;
}

int oas_schema_set_description(oas_schema_t *schema, const char *description)
{
    if (!schema) {
        return -EINVAL;
    }
    schema->description = description;
    return 0;
}

int oas_schema_set_additional_properties(oas_schema_t *schema, oas_schema_t *additional)
{
    if (!schema) {
        return -EINVAL;
    }
    schema->additional_properties = additional;
    return 0;
}
