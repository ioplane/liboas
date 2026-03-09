#include <liboas/oas_validator.h>

#include "compiler/oas_format.h"
#include "compiler/oas_instruction.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <yyjson.h>

/* Access the program from compiled schema — program is the first field */
static const oas_program_t *get_program(const oas_compiled_schema_t *cs)
{
    return (const oas_program_t *)cs;
}

/* Access the regex backend — second field after program */
static oas_regex_backend_t *get_regex(const oas_compiled_schema_t *cs)
{
    const struct {
        oas_program_t program;
        oas_regex_backend_t *regex;
    } *p = (const void *)cs;
    return p->regex;
}

static uint8_t yyjson_to_type_mask(yyjson_val *val)
{
    if (yyjson_is_null(val))
        return OAS_TYPE_NULL;
    if (yyjson_is_bool(val))
        return OAS_TYPE_BOOLEAN;
    if (yyjson_is_int(val) || yyjson_is_sint(val) || yyjson_is_uint(val))
        return OAS_TYPE_INTEGER | OAS_TYPE_NUMBER;
    if (yyjson_is_real(val)) {
        double d = yyjson_get_real(val);
        double intpart;
        if (modf(d, &intpart) == 0.0)
            return OAS_TYPE_INTEGER | OAS_TYPE_NUMBER;
        return OAS_TYPE_NUMBER;
    }
    if (yyjson_is_str(val))
        return OAS_TYPE_STRING;
    if (yyjson_is_arr(val))
        return OAS_TYPE_ARRAY;
    if (yyjson_is_obj(val))
        return OAS_TYPE_OBJECT;
    return 0;
}

static size_t utf8_codepoint_count(const char *s, size_t byte_len)
{
    size_t count = 0;
    for (size_t i = 0; i < byte_len; i++) {
        if ((s[i] & 0xC0) != 0x80)
            count++;
    }
    return count;
}

static bool has_duplicate_items(yyjson_val *arr)
{
    size_t len = yyjson_arr_size(arr);
    for (size_t i = 0; i < len; i++) {
        yyjson_val *a = yyjson_arr_get(arr, i);
        for (size_t j = i + 1; j < len; j++) {
            yyjson_val *b = yyjson_arr_get(arr, j);
            if (yyjson_equals(a, b))
                return true;
        }
    }
    return false;
}

typedef struct {
    yyjson_val *value;
    const oas_instruction_t *ip;
    const oas_instruction_t *end;
    oas_error_list_t *errors;
    oas_arena_t *arena;
    oas_regex_backend_t *regex;
    bool valid;
} vm_state_t;

static void add_error(vm_state_t *vm, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

static void add_error(vm_state_t *vm, const char *fmt, ...)
{
    vm->valid = false;
    if (vm->errors) {
        va_list ap;
        va_start(ap, fmt);
        /* Build message on stack, then pass to error list */
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        oas_error_list_add(vm->errors, OAS_ERR_CONSTRAINT, "", "%s", buf);
    }
}

static void execute(vm_state_t *vm);

static bool is_nesting_op(oas_opcode_t op)
{
    return op == OAS_OP_ENTER_ITEMS || op == OAS_OP_ENTER_PROPERTY ||
           op == OAS_OP_ENTER_PREFIX_ITEM || op == OAS_OP_ENTER_ADDITIONAL ||
           op == OAS_OP_NEGATE || op == OAS_OP_COND_IF || op == OAS_OP_COND_THEN ||
           op == OAS_OP_COND_ELSE;
}

static void skip_to_end(vm_state_t *vm)
{
    int depth = 1;
    while (vm->ip < vm->end && depth > 0) {
        oas_opcode_t op = vm->ip->op;
        vm->ip++;
        if (op == OAS_OP_END) {
            depth--;
        } else if (is_nesting_op(op)) {
            depth++;
        }
    }
}

static void execute(vm_state_t *vm)
{
    while (vm->ip < vm->end) {
        const oas_instruction_t *instr = vm->ip++;

        switch (instr->op) {
        case OAS_OP_END:
            return;

        case OAS_OP_NOP:
            break;

        case OAS_OP_CHECK_TYPE: {
            uint8_t actual = yyjson_to_type_mask(vm->value);
            if (!(actual & instr->type_mask))
                add_error(vm, "type mismatch");
            break;
        }

        case OAS_OP_CHECK_MIN_LEN: {
            if (!yyjson_is_str(vm->value))
                break;
            const char *s = yyjson_get_str(vm->value);
            size_t len = utf8_codepoint_count(s, yyjson_get_len(vm->value));
            if ((int64_t)len < instr->operand.i64)
                add_error(vm, "string too short");
            break;
        }

        case OAS_OP_CHECK_MAX_LEN: {
            if (!yyjson_is_str(vm->value))
                break;
            const char *s = yyjson_get_str(vm->value);
            size_t len = utf8_codepoint_count(s, yyjson_get_len(vm->value));
            if ((int64_t)len > instr->operand.i64)
                add_error(vm, "string too long");
            break;
        }

        case OAS_OP_CHECK_PATTERN: {
            if (!yyjson_is_str(vm->value) || !vm->regex)
                break;
            const char *s = yyjson_get_str(vm->value);
            size_t slen = yyjson_get_len(vm->value);
            oas_compiled_pattern_t *pat = instr->operand.ptr;
            if (!vm->regex->match(vm->regex, pat, s, slen))
                add_error(vm, "pattern mismatch");
            break;
        }

        case OAS_OP_CHECK_FORMAT: {
            if (!yyjson_is_str(vm->value))
                break;
            const char *s = yyjson_get_str(vm->value);
            size_t slen = yyjson_get_len(vm->value);
            oas_format_fn_t fn = (oas_format_fn_t)(uintptr_t)instr->operand.ptr;
            if (!fn(s, slen))
                add_error(vm, "format validation failed");
            break;
        }

        case OAS_OP_CHECK_MINIMUM: {
            if (!yyjson_is_num(vm->value))
                break;
            double v = yyjson_get_num(vm->value);
            if (v < instr->operand.f64)
                add_error(vm, "below minimum");
            break;
        }

        case OAS_OP_CHECK_MAXIMUM: {
            if (!yyjson_is_num(vm->value))
                break;
            double v = yyjson_get_num(vm->value);
            if (v > instr->operand.f64)
                add_error(vm, "above maximum");
            break;
        }

        case OAS_OP_CHECK_EX_MINIMUM: {
            if (!yyjson_is_num(vm->value))
                break;
            double v = yyjson_get_num(vm->value);
            if (v <= instr->operand.f64)
                add_error(vm, "not above exclusive minimum");
            break;
        }

        case OAS_OP_CHECK_EX_MAXIMUM: {
            if (!yyjson_is_num(vm->value))
                break;
            double v = yyjson_get_num(vm->value);
            if (v >= instr->operand.f64)
                add_error(vm, "not below exclusive maximum");
            break;
        }

        case OAS_OP_CHECK_MULTIPLE_OF: {
            if (!yyjson_is_num(vm->value))
                break;
            double v = yyjson_get_num(vm->value);
            double m = instr->operand.f64;
            double rem = fmod(v, m);
            if (fabs(rem) > 1e-9 && fabs(rem - m) > 1e-9)
                add_error(vm, "not multiple of");
            break;
        }

        case OAS_OP_CHECK_REQUIRED: {
            if (!yyjson_is_obj(vm->value))
                break;
            const char *name = instr->operand.str;
            if (!yyjson_obj_get(vm->value, name))
                add_error(vm, "missing required property: %s", name);
            break;
        }

        case OAS_OP_CHECK_MIN_ITEMS: {
            if (!yyjson_is_arr(vm->value))
                break;
            size_t n = yyjson_arr_size(vm->value);
            if ((int64_t)n < instr->operand.i64)
                add_error(vm, "too few items");
            break;
        }

        case OAS_OP_CHECK_MAX_ITEMS: {
            if (!yyjson_is_arr(vm->value))
                break;
            size_t n = yyjson_arr_size(vm->value);
            if ((int64_t)n > instr->operand.i64)
                add_error(vm, "too many items");
            break;
        }

        case OAS_OP_CHECK_UNIQUE: {
            if (!yyjson_is_arr(vm->value))
                break;
            if (has_duplicate_items(vm->value))
                add_error(vm, "array has duplicate items");
            break;
        }

        case OAS_OP_ENTER_ITEMS: {
            if (!yyjson_is_arr(vm->value)) {
                skip_to_end(vm);
                break;
            }
            const oas_instruction_t *sub_start = vm->ip;
            yyjson_val *item;
            yyjson_arr_iter iter;
            yyjson_arr_iter_init(vm->value, &iter);
            while ((item = yyjson_arr_iter_next(&iter)) != nullptr) {
                vm_state_t sub = *vm;
                sub.value = item;
                sub.ip = sub_start;
                execute(&sub);
                if (!sub.valid)
                    vm->valid = false;
            }
            skip_to_end(vm);
            break;
        }

        case OAS_OP_ENTER_PREFIX_ITEM: {
            if (!yyjson_is_arr(vm->value)) {
                skip_to_end(vm);
                break;
            }
            uint16_t idx = instr->operand.branch.index;
            yyjson_val *item = yyjson_arr_get(vm->value, idx);
            if (item) {
                vm_state_t sub = *vm;
                sub.value = item;
                execute(&sub);
                if (!sub.valid)
                    vm->valid = false;
            }
            skip_to_end(vm);
            break;
        }

        case OAS_OP_ENTER_PROPERTY: {
            if (!yyjson_is_obj(vm->value)) {
                skip_to_end(vm);
                break;
            }
            const char *name = instr->operand.str;
            yyjson_val *prop = yyjson_obj_get(vm->value, name);
            if (prop) {
                vm_state_t sub = *vm;
                sub.value = prop;
                execute(&sub);
                if (!sub.valid)
                    vm->valid = false;
            }
            skip_to_end(vm);
            break;
        }

        case OAS_OP_ENTER_ADDITIONAL: {
            skip_to_end(vm);
            break;
        }

        case OAS_OP_BRANCH_ALLOF: {
            uint16_t count = instr->operand.branch.count;
            for (uint16_t i = 0; i < count; i++) {
                vm_state_t sub = *vm;
                execute(&sub);
                if (!sub.valid)
                    vm->valid = false;
                vm->ip = sub.ip;
            }
            break;
        }

        case OAS_OP_BRANCH_ANYOF: {
            uint16_t count = instr->operand.branch.count;
            bool any_pass = false;
            const oas_instruction_t *after = nullptr;
            for (uint16_t i = 0; i < count; i++) {
                vm_state_t sub = *vm;
                sub.errors = nullptr;
                execute(&sub);
                after = sub.ip;
                if (sub.valid)
                    any_pass = true;
                vm->ip = sub.ip;
            }
            if (!any_pass)
                add_error(vm, "no anyOf branch matched");
            if (after)
                vm->ip = after;
            break;
        }

        case OAS_OP_BRANCH_ONEOF: {
            uint16_t count = instr->operand.branch.count;
            int pass_count = 0;
            const oas_instruction_t *after = nullptr;
            for (uint16_t i = 0; i < count; i++) {
                vm_state_t sub = *vm;
                sub.errors = nullptr;
                execute(&sub);
                after = sub.ip;
                if (sub.valid)
                    pass_count++;
                vm->ip = sub.ip;
            }
            if (pass_count != 1)
                add_error(vm, "oneOf: expected exactly 1 match, got %d", pass_count);
            if (after)
                vm->ip = after;
            break;
        }

        case OAS_OP_NEGATE: {
            vm_state_t sub = *vm;
            sub.errors = nullptr;
            execute(&sub);
            vm->ip = sub.ip;
            if (sub.valid)
                add_error(vm, "not: schema matched when it should not");
            break;
        }

        case OAS_OP_COND_IF: {
            vm_state_t sub = *vm;
            sub.errors = nullptr;
            execute(&sub);
            bool if_passed = sub.valid;
            vm->ip = sub.ip;

            if (vm->ip < vm->end && vm->ip->op == OAS_OP_COND_THEN) {
                vm->ip++;
                if (if_passed) {
                    execute(vm);
                } else {
                    skip_to_end(vm);
                }
            }
            if (vm->ip < vm->end && vm->ip->op == OAS_OP_COND_ELSE) {
                vm->ip++;
                if (!if_passed) {
                    execute(vm);
                } else {
                    skip_to_end(vm);
                }
            }
            break;
        }

        case OAS_OP_CHECK_ENUM: {
            yyjson_val *enum_arr = instr->operand.ptr;
            if (!yyjson_is_arr(enum_arr))
                break;
            bool found = false;
            yyjson_val *item;
            yyjson_arr_iter iter;
            yyjson_arr_iter_init(enum_arr, &iter);
            while ((item = yyjson_arr_iter_next(&iter)) != nullptr) {
                if (yyjson_equals(vm->value, item)) {
                    found = true;
                    break;
                }
            }
            if (!found)
                add_error(vm, "value not in enum");
            break;
        }

        case OAS_OP_CHECK_CONST: {
            yyjson_val *expected = instr->operand.ptr;
            if (!yyjson_equals(vm->value, expected))
                add_error(vm, "value does not match const");
            break;
        }

        default:
            break;
        }
    }
}

int oas_validate(const oas_compiled_schema_t *compiled, yyjson_val *value,
                 oas_validation_result_t *result, oas_arena_t *arena)
{
    if (!compiled || !value || !result)
        return -EINVAL;

    const oas_program_t *prog = get_program(compiled);
    if (!prog->code || prog->count == 0)
        return -EINVAL;

    result->valid = true;
    if (!result->errors && arena) {
        result->errors = oas_error_list_create(arena);
    }

    vm_state_t vm = {
        .value = value,
        .ip = prog->code,
        .end = prog->code + prog->count,
        .errors = result->errors,
        .arena = arena,
        .regex = get_regex(compiled),
        .valid = true,
    };

    execute(&vm);

    result->valid = vm.valid;
    return 0;
}

int oas_validate_json(const oas_compiled_schema_t *compiled, const char *json, size_t len,
                      oas_validation_result_t *result, oas_arena_t *arena)
{
    if (!compiled || !json || !result)
        return -EINVAL;

    yyjson_doc *doc = yyjson_read(json, len, 0);
    if (!doc)
        return -EINVAL;

    yyjson_val *root = yyjson_doc_get_root(doc);
    int rc = oas_validate(compiled, root, result, arena);

    yyjson_doc_free(doc);
    return rc;
}
