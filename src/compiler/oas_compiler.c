#include <liboas/oas_compiler.h>

#include "oas_format.h"
#include "oas_instruction.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr size_t OAS_PATTERN_INITIAL_CAP = 8;

struct oas_compiled_schema {
    oas_program_t program;
    oas_regex_backend_t *regex;
    oas_compiled_pattern_t **patterns;
    size_t pattern_count;
    size_t pattern_capacity;
};

static int track_pattern(oas_compiled_schema_t *cs, oas_compiled_pattern_t *pat)
{
    if (cs->pattern_count >= cs->pattern_capacity) {
        size_t new_cap = cs->pattern_capacity == 0 ? OAS_PATTERN_INITIAL_CAP
                                                   : cs->pattern_capacity * 2;
        oas_compiled_pattern_t **new_arr = realloc(cs->patterns, new_cap * sizeof(*new_arr));
        if (!new_arr) {
            return -ENOMEM;
        }
        cs->patterns = new_arr;
        cs->pattern_capacity = new_cap;
    }
    cs->patterns[cs->pattern_count++] = pat;
    return 0;
}

static int emit_simple(oas_program_t *prog, oas_opcode_t op)
{
    oas_instruction_t instr = {.op = op};
    return oas_program_emit(prog, &instr);
}

static int emit_i64(oas_program_t *prog, oas_opcode_t op, int64_t val)
{
    oas_instruction_t instr = {.op = op};
    instr.operand.i64 = val;
    return oas_program_emit(prog, &instr);
}

static int emit_f64(oas_program_t *prog, oas_opcode_t op, double val)
{
    oas_instruction_t instr = {.op = op};
    instr.operand.f64 = val;
    return oas_program_emit(prog, &instr);
}

static int emit_str(oas_program_t *prog, oas_opcode_t op, const char *str)
{
    oas_instruction_t instr = {.op = op};
    instr.operand.str = str;
    return oas_program_emit(prog, &instr);
}

static int emit_ptr(oas_program_t *prog, oas_opcode_t op, void *ptr)
{
    oas_instruction_t instr = {.op = op};
    instr.operand.ptr = ptr;
    return oas_program_emit(prog, &instr);
}

static int emit_type(oas_program_t *prog, uint8_t mask)
{
    oas_instruction_t instr = {.op = OAS_OP_CHECK_TYPE, .type_mask = mask};
    return oas_program_emit(prog, &instr);
}

static int emit_branch(oas_program_t *prog, oas_opcode_t op, uint16_t count)
{
    oas_instruction_t instr = {.op = op};
    instr.operand.branch.count = count;
    instr.operand.branch.index = 0;
    return oas_program_emit(prog, &instr);
}

static int emit_prefix_item(oas_program_t *prog, uint16_t index)
{
    oas_instruction_t instr = {.op = OAS_OP_ENTER_PREFIX_ITEM};
    instr.operand.branch.index = index;
    return oas_program_emit(prog, &instr);
}

static int compile_schema(oas_compiled_schema_t *cs, const oas_schema_t *schema,
                          const oas_compiler_config_t *config, oas_error_list_t *errors);

static int compile_composition(oas_compiled_schema_t *cs, oas_opcode_t op, oas_schema_t **schemas,
                               size_t count, const oas_compiler_config_t *config,
                               oas_error_list_t *errors)
{
    int rc = emit_branch(&cs->program, op, (uint16_t)count);
    if (rc < 0) {
        return rc;
    }

    for (size_t i = 0; i < count; i++) {
        rc = compile_schema(cs, schemas[i], config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = emit_simple(&cs->program, OAS_OP_END);
        if (rc < 0) {
            return rc;
        }
    }
    return 0;
}

static int compile_conditional_part(oas_compiled_schema_t *cs, oas_opcode_t op,
                                    const oas_schema_t *schema, const oas_compiler_config_t *config,
                                    oas_error_list_t *errors)
{
    if (!schema) {
        return 0;
    }

    int rc = emit_simple(&cs->program, op);
    if (rc < 0) {
        return rc;
    }
    rc = compile_schema(cs, schema, config, errors);
    if (rc < 0) {
        return rc;
    }
    return emit_simple(&cs->program, OAS_OP_END);
}

static int compile_schema(oas_compiled_schema_t *cs, const oas_schema_t *schema,
                          const oas_compiler_config_t *config, oas_error_list_t *errors)
{
    if (!schema) {
        return 0;
    }

    /* Follow $ref if resolved */
    if (schema->ref_resolved) {
        return compile_schema(cs, schema->ref_resolved, config, errors);
    }

    /* Unresolved $ref — cannot compile this schema */
    if (schema->ref) {
        return -EINVAL;
    }

    int rc;
    oas_program_t *prog = &cs->program;

    /* Type check (with nullable support) */
    uint8_t effective_mask = schema->type_mask;
    if (schema->nullable && effective_mask != 0) {
        effective_mask |= OAS_TYPE_NULL;
    }
    if (effective_mask != 0) {
        rc = emit_type(prog, effective_mask);
        if (rc < 0) {
            return rc;
        }
    }

    /* String constraints */
    if (schema->min_length >= 0) {
        rc = emit_i64(prog, OAS_OP_CHECK_MIN_LEN, schema->min_length);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->max_length >= 0) {
        rc = emit_i64(prog, OAS_OP_CHECK_MAX_LEN, schema->max_length);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->pattern) {
        if (cs->regex) {
            oas_compiled_pattern_t *compiled_pat = nullptr;
            rc = cs->regex->compile(cs->regex, schema->pattern, &compiled_pat);
            if (rc < 0) {
                if (errors) {
                    oas_error_list_add(errors, OAS_ERR_CONSTRAINT, "",
                                       "Failed to compile pattern: %s", schema->pattern);
                }
                return rc;
            }
            rc = track_pattern(cs, compiled_pat);
            if (rc < 0) {
                cs->regex->free_pattern(cs->regex, compiled_pat);
                return rc;
            }
            rc = emit_ptr(prog, OAS_OP_CHECK_PATTERN, compiled_pat);
            if (rc < 0) {
                return rc;
            }
        }
    }
    if (schema->format && config &&
        (oas_format_policy_t)config->format_policy != OAS_FORMAT_IGNORE) {
        oas_format_fn_t fn = oas_format_get(schema->format);
        if (fn) {
            rc = emit_ptr(prog, OAS_OP_CHECK_FORMAT, (void *)(uintptr_t)fn);
            if (rc < 0) {
                return rc;
            }
        }
    }

    /* Numeric constraints */
    if (schema->has_minimum) {
        rc = emit_f64(prog, OAS_OP_CHECK_MINIMUM, schema->minimum);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->has_maximum) {
        rc = emit_f64(prog, OAS_OP_CHECK_MAXIMUM, schema->maximum);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->has_exclusive_minimum) {
        rc = emit_f64(prog, OAS_OP_CHECK_EX_MINIMUM, schema->exclusive_minimum);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->has_exclusive_maximum) {
        rc = emit_f64(prog, OAS_OP_CHECK_EX_MAXIMUM, schema->exclusive_maximum);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->has_multiple_of) {
        rc = emit_f64(prog, OAS_OP_CHECK_MULTIPLE_OF, schema->multiple_of);
        if (rc < 0) {
            return rc;
        }
    }

    /* Array constraints */
    if (schema->min_items >= 0) {
        rc = emit_i64(prog, OAS_OP_CHECK_MIN_ITEMS, schema->min_items);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->max_items >= 0) {
        rc = emit_i64(prog, OAS_OP_CHECK_MAX_ITEMS, schema->max_items);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->unique_items) {
        rc = emit_simple(prog, OAS_OP_CHECK_UNIQUE);
        if (rc < 0) {
            return rc;
        }
    }
    for (size_t i = 0; i < schema->prefix_items_count; i++) {
        rc = emit_prefix_item(prog, (uint16_t)i);
        if (rc < 0) {
            return rc;
        }
        rc = compile_schema(cs, schema->prefix_items[i], config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = emit_simple(prog, OAS_OP_END);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->items) {
        rc = emit_simple(prog, OAS_OP_ENTER_ITEMS);
        if (rc < 0) {
            return rc;
        }
        rc = compile_schema(cs, schema->items, config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = emit_simple(prog, OAS_OP_END);
        if (rc < 0) {
            return rc;
        }
    }

    /* Object constraints */
    for (size_t i = 0; i < schema->required_count; i++) {
        rc = emit_str(prog, OAS_OP_CHECK_REQUIRED, schema->required[i]);
        if (rc < 0) {
            return rc;
        }
    }
    for (oas_property_t *prop = schema->properties; prop; prop = prop->next) {
        rc = emit_str(prog, OAS_OP_ENTER_PROPERTY, prop->name);
        if (rc < 0) {
            return rc;
        }
        rc = compile_schema(cs, prop->schema, config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = emit_simple(prog, OAS_OP_END);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->additional_properties) {
        rc = emit_simple(prog, OAS_OP_ENTER_ADDITIONAL);
        if (rc < 0) {
            return rc;
        }
        rc = compile_schema(cs, schema->additional_properties, config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = emit_simple(prog, OAS_OP_END);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->min_properties >= 0) {
        rc = emit_i64(prog, OAS_OP_CHECK_MIN_PROPS, schema->min_properties);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->max_properties >= 0) {
        rc = emit_i64(prog, OAS_OP_CHECK_MAX_PROPS, schema->max_properties);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->property_names) {
        rc = emit_simple(prog, OAS_OP_CHECK_PROP_NAMES);
        if (rc < 0) {
            return rc;
        }
        rc = compile_schema(cs, schema->property_names, config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = emit_simple(prog, OAS_OP_END);
        if (rc < 0) {
            return rc;
        }
    }
    for (size_t i = 0; i < schema->pattern_properties_count; i++) {
        if (!schema->pattern_properties[i].pattern || !schema->pattern_properties[i].schema) {
            continue;
        }
        if (cs->regex) {
            oas_compiled_pattern_t *compiled_pat = nullptr;
            rc =
                cs->regex->compile(cs->regex, schema->pattern_properties[i].pattern, &compiled_pat);
            if (rc < 0) {
                if (errors) {
                    oas_error_list_add(errors, OAS_ERR_CONSTRAINT, "",
                                       "Failed to compile patternProperties pattern: %s",
                                       schema->pattern_properties[i].pattern);
                }
                return rc;
            }
            rc = track_pattern(cs, compiled_pat);
            if (rc < 0) {
                cs->regex->free_pattern(cs->regex, compiled_pat);
                return rc;
            }
            rc = emit_ptr(prog, OAS_OP_CHECK_PATTERN_PROPS, compiled_pat);
            if (rc < 0) {
                return rc;
            }
            rc = compile_schema(cs, schema->pattern_properties[i].schema, config, errors);
            if (rc < 0) {
                return rc;
            }
            rc = emit_simple(prog, OAS_OP_END);
            if (rc < 0) {
                return rc;
            }
        }
    }
    for (size_t i = 0; i < schema->dependent_required_count; i++) {
        /* Encode: operand.str = trigger property, then inline the required names */
        rc = emit_str(prog, OAS_OP_CHECK_DEP_REQUIRED, schema->dependent_required[i].property);
        if (rc < 0) {
            return rc;
        }
        /* Store count + required names as subsequent instructions */
        rc = emit_i64(prog, OAS_OP_NOP, (int64_t)schema->dependent_required[i].required_count);
        if (rc < 0) {
            return rc;
        }
        for (size_t j = 0; j < schema->dependent_required[i].required_count; j++) {
            rc = emit_str(prog, OAS_OP_NOP, schema->dependent_required[i].required[j]);
            if (rc < 0) {
                return rc;
            }
        }
    }
    for (size_t i = 0; i < schema->dependent_schemas_count; i++) {
        if (!schema->dependent_schemas[i].schema) {
            continue;
        }
        rc = emit_str(prog, OAS_OP_CHECK_DEP_SCHEMA, schema->dependent_schemas[i].property);
        if (rc < 0) {
            return rc;
        }
        rc = compile_schema(cs, schema->dependent_schemas[i].schema, config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = emit_simple(prog, OAS_OP_END);
        if (rc < 0) {
            return rc;
        }
    }

    /* Array: contains */
    if (schema->contains) {
        /* Encode min/max contains as i64 operands in the CHECK_CONTAINS instruction */
        oas_instruction_t cinstr = {.op = OAS_OP_CHECK_CONTAINS};
        /* Pack min_contains into branch.index and max_contains into offset
         * Actually, use separate approach: emit CHECK_CONTAINS with ptr to sub-program,
         * followed by compiled contains schema + END, and store min/max inline */
        int64_t min_c = schema->min_contains >= 0 ? schema->min_contains : 1;
        int64_t max_c = schema->max_contains;
        cinstr.operand.i64 = min_c;
        rc = oas_program_emit(prog, &cinstr);
        if (rc < 0) {
            return rc;
        }
        /* Emit max_contains as NOP */
        rc = emit_i64(prog, OAS_OP_NOP, max_c);
        if (rc < 0) {
            return rc;
        }
        rc = compile_schema(cs, schema->contains, config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = emit_simple(prog, OAS_OP_END);
        if (rc < 0) {
            return rc;
        }
    }

    /* Composition (with discriminator optimization) */
    if (schema->all_of && schema->all_of_count > 0) {
        rc = compile_composition(cs, OAS_OP_BRANCH_ALLOF, schema->all_of, schema->all_of_count,
                                 config, errors);
        if (rc < 0) {
            return rc;
        }
    }

    /* Discriminator replaces brute-force anyOf/oneOf when mapping is available */
    bool has_discriminator = schema->discriminator_property &&
                             schema->discriminator_mapping_count > 0;

    if (has_discriminator) {
        oas_schema_t **branches = nullptr;
        size_t branch_count = 0;
        if (schema->one_of && schema->one_of_count > 0) {
            branches = schema->one_of;
            branch_count = schema->one_of_count;
        } else if (schema->any_of && schema->any_of_count > 0) {
            branches = schema->any_of;
            branch_count = schema->any_of_count;
        }
        if (branches && branch_count > 0 && branch_count == schema->discriminator_mapping_count) {
            rc = emit_str(prog, OAS_OP_DISCRIMINATOR, schema->discriminator_property);
            if (rc < 0) {
                return rc;
            }
            rc = emit_i64(prog, OAS_OP_NOP, (int64_t)schema->discriminator_mapping_count);
            if (rc < 0) {
                return rc;
            }
            for (size_t i = 0; i < schema->discriminator_mapping_count; i++) {
                rc = emit_str(prog, OAS_OP_NOP, schema->discriminator_mapping[i].key);
                if (rc < 0) {
                    return rc;
                }
            }
            for (size_t i = 0; i < branch_count; i++) {
                rc = compile_schema(cs, branches[i], config, errors);
                if (rc < 0) {
                    return rc;
                }
                rc = emit_simple(prog, OAS_OP_END);
                if (rc < 0) {
                    return rc;
                }
            }
        }
    } else {
        if (schema->any_of && schema->any_of_count > 0) {
            rc = compile_composition(cs, OAS_OP_BRANCH_ANYOF, schema->any_of, schema->any_of_count,
                                     config, errors);
            if (rc < 0) {
                return rc;
            }
        }
        if (schema->one_of && schema->one_of_count > 0) {
            rc = compile_composition(cs, OAS_OP_BRANCH_ONEOF, schema->one_of, schema->one_of_count,
                                     config, errors);
            if (rc < 0) {
                return rc;
            }
        }
    }

    /* Not */
    if (schema->not_schema) {
        rc = emit_simple(prog, OAS_OP_NEGATE);
        if (rc < 0) {
            return rc;
        }
        rc = compile_schema(cs, schema->not_schema, config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = emit_simple(prog, OAS_OP_END);
        if (rc < 0) {
            return rc;
        }
    }

    /* Conditional */
    if (schema->if_schema) {
        rc = compile_conditional_part(cs, OAS_OP_COND_IF, schema->if_schema, config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = compile_conditional_part(cs, OAS_OP_COND_THEN, schema->then_schema, config, errors);
        if (rc < 0) {
            return rc;
        }
        rc = compile_conditional_part(cs, OAS_OP_COND_ELSE, schema->else_schema, config, errors);
        if (rc < 0) {
            return rc;
        }
    }

    /* Enum / Const */
    if (schema->enum_values) {
        rc = emit_ptr(prog, OAS_OP_CHECK_ENUM, schema->enum_values);
        if (rc < 0) {
            return rc;
        }
    }
    if (schema->const_value) {
        rc = emit_ptr(prog, OAS_OP_CHECK_CONST, schema->const_value);
        if (rc < 0) {
            return rc;
        }
    }

    return 0;
}

oas_compiled_schema_t *oas_schema_compile(const oas_schema_t *schema,
                                          const oas_compiler_config_t *config,
                                          oas_error_list_t *errors)
{
    if (!schema) {
        return nullptr;
    }

    oas_compiled_schema_t *cs = calloc(1, sizeof(*cs));
    if (!cs) {
        return nullptr;
    }

    oas_program_init(&cs->program);
    cs->regex = config ? config->regex : nullptr;

    int rc = compile_schema(cs, schema, config, errors);
    if (rc < 0) {
        oas_compiled_schema_free(cs);
        return nullptr;
    }

    rc = emit_simple(&cs->program, OAS_OP_END);
    if (rc < 0) {
        oas_compiled_schema_free(cs);
        return nullptr;
    }

    return cs;
}

void oas_compiled_schema_free(oas_compiled_schema_t *compiled)
{
    if (!compiled) {
        return;
    }

    if (compiled->regex && compiled->patterns) {
        for (size_t i = 0; i < compiled->pattern_count; i++) {
            compiled->regex->free_pattern(compiled->regex, compiled->patterns[i]);
        }
    }
    free(compiled->patterns);
    oas_program_destroy(&compiled->program);
    free(compiled);
}
