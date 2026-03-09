#include "oas_instruction.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

constexpr size_t OAS_PROGRAM_INITIAL_CAP = 32;

void oas_program_init(oas_program_t *prog)
{
    if (!prog) {
        return;
    }
    prog->code = nullptr;
    prog->count = 0;
    prog->capacity = 0;
}

void oas_program_destroy(oas_program_t *prog)
{
    if (!prog) {
        return;
    }
    free(prog->code);
    prog->code = nullptr;
    prog->count = 0;
    prog->capacity = 0;
}

int oas_program_emit(oas_program_t *prog, const oas_instruction_t *instr)
{
    if (!prog || !instr) {
        return -EINVAL;
    }

    if (prog->count >= prog->capacity) {
        size_t new_cap = prog->capacity == 0 ? OAS_PROGRAM_INITIAL_CAP : prog->capacity * 2;
        oas_instruction_t *new_code = realloc(prog->code, new_cap * sizeof(*new_code));
        if (!new_code) {
            return -ENOMEM;
        }
        prog->code = new_code;
        prog->capacity = new_cap;
    }

    prog->code[prog->count++] = *instr;
    return 0;
}

size_t oas_program_pos(const oas_program_t *prog)
{
    if (!prog) {
        return 0;
    }
    return prog->count;
}

void oas_program_patch(oas_program_t *prog, size_t pos, size_t target)
{
    if (!prog || pos >= prog->count) {
        return;
    }
    prog->code[pos].operand.offset = target;
}

void oas_program_reset(oas_program_t *prog)
{
    if (!prog) {
        return;
    }
    prog->count = 0;
}

static const char *const opcode_names[] = {
    [OAS_OP_NOP] = "NOP",
    [OAS_OP_CHECK_TYPE] = "CHECK_TYPE",
    [OAS_OP_CHECK_MIN_LEN] = "CHECK_MIN_LEN",
    [OAS_OP_CHECK_MAX_LEN] = "CHECK_MAX_LEN",
    [OAS_OP_CHECK_PATTERN] = "CHECK_PATTERN",
    [OAS_OP_CHECK_FORMAT] = "CHECK_FORMAT",
    [OAS_OP_CHECK_MINIMUM] = "CHECK_MINIMUM",
    [OAS_OP_CHECK_MAXIMUM] = "CHECK_MAXIMUM",
    [OAS_OP_CHECK_EX_MINIMUM] = "CHECK_EX_MINIMUM",
    [OAS_OP_CHECK_EX_MAXIMUM] = "CHECK_EX_MAXIMUM",
    [OAS_OP_CHECK_MULTIPLE_OF] = "CHECK_MULTIPLE_OF",
    [OAS_OP_CHECK_ENUM] = "CHECK_ENUM",
    [OAS_OP_CHECK_CONST] = "CHECK_CONST",
    [OAS_OP_CHECK_REQUIRED] = "CHECK_REQUIRED",
    [OAS_OP_CHECK_MIN_ITEMS] = "CHECK_MIN_ITEMS",
    [OAS_OP_CHECK_MAX_ITEMS] = "CHECK_MAX_ITEMS",
    [OAS_OP_CHECK_UNIQUE] = "CHECK_UNIQUE",
    [OAS_OP_ENTER_PROPERTY] = "ENTER_PROPERTY",
    [OAS_OP_ENTER_ITEMS] = "ENTER_ITEMS",
    [OAS_OP_ENTER_PREFIX_ITEM] = "ENTER_PREFIX_ITEM",
    [OAS_OP_ENTER_ADDITIONAL] = "ENTER_ADDITIONAL",
    [OAS_OP_BRANCH_ALLOF] = "BRANCH_ALLOF",
    [OAS_OP_BRANCH_ANYOF] = "BRANCH_ANYOF",
    [OAS_OP_BRANCH_ONEOF] = "BRANCH_ONEOF",
    [OAS_OP_NEGATE] = "NEGATE",
    [OAS_OP_COND_IF] = "COND_IF",
    [OAS_OP_COND_THEN] = "COND_THEN",
    [OAS_OP_COND_ELSE] = "COND_ELSE",
    [OAS_OP_DISCRIMINATOR] = "DISCRIMINATOR",
    [OAS_OP_JUMP] = "JUMP",
    [OAS_OP_JUMP_IF_FAIL] = "JUMP_IF_FAIL",
    [OAS_OP_PUSH_SCOPE] = "PUSH_SCOPE",
    [OAS_OP_POP_SCOPE] = "POP_SCOPE",
    [OAS_OP_CHECK_UNEVAL_PROPS] = "CHECK_UNEVAL_PROPS",
    [OAS_OP_CHECK_UNEVAL_ITEMS] = "CHECK_UNEVAL_ITEMS",
    [OAS_OP_END] = "END",
};

const char *oas_opcode_name(oas_opcode_t op)
{
    if (op >= OAS_OP_COUNT_) {
        return "UNKNOWN";
    }
    return opcode_names[op];
}
