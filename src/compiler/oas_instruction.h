/**
 * @file oas_instruction.h
 * @brief Validation instruction set and program builder.
 *
 * Defines the bytecode instruction set for schema validation.
 * Instructions are emitted by the compiler and executed by the VM.
 */

#ifndef LIBOAS_COMPILER_OAS_INSTRUCTION_H
#define LIBOAS_COMPILER_OAS_INSTRUCTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum : uint8_t {
    OAS_OP_NOP = 0,
    OAS_OP_CHECK_TYPE,
    OAS_OP_CHECK_MIN_LEN,
    OAS_OP_CHECK_MAX_LEN,
    OAS_OP_CHECK_PATTERN,
    OAS_OP_CHECK_FORMAT,
    OAS_OP_CHECK_MINIMUM,
    OAS_OP_CHECK_MAXIMUM,
    OAS_OP_CHECK_EX_MINIMUM,
    OAS_OP_CHECK_EX_MAXIMUM,
    OAS_OP_CHECK_MULTIPLE_OF,
    OAS_OP_CHECK_ENUM,
    OAS_OP_CHECK_CONST,
    OAS_OP_CHECK_REQUIRED,
    OAS_OP_CHECK_MIN_ITEMS,
    OAS_OP_CHECK_MAX_ITEMS,
    OAS_OP_CHECK_UNIQUE,
    OAS_OP_ENTER_PROPERTY,
    OAS_OP_ENTER_ITEMS,
    OAS_OP_ENTER_PREFIX_ITEM,
    OAS_OP_ENTER_ADDITIONAL,
    OAS_OP_BRANCH_ALLOF,
    OAS_OP_BRANCH_ANYOF,
    OAS_OP_BRANCH_ONEOF,
    OAS_OP_NEGATE,
    OAS_OP_COND_IF,
    OAS_OP_COND_THEN,
    OAS_OP_COND_ELSE,
    OAS_OP_DISCRIMINATOR,
    OAS_OP_JUMP,
    OAS_OP_JUMP_IF_FAIL,
    OAS_OP_PUSH_SCOPE,
    OAS_OP_POP_SCOPE,
    OAS_OP_CHECK_UNEVAL_PROPS,
    OAS_OP_CHECK_UNEVAL_ITEMS,
    OAS_OP_END,
    OAS_OP_COUNT_,
} oas_opcode_t;

typedef struct {
    oas_opcode_t op;
    uint8_t type_mask;
    uint16_t _pad;
    union {
        int64_t i64;
        double f64;
        const char *str;
        size_t offset;
        void *ptr;
        struct {
            uint16_t count;
            uint16_t index;
        } branch;
    } operand;
} oas_instruction_t;

typedef struct {
    oas_instruction_t *code;
    size_t count;
    size_t capacity;
} oas_program_t;

/**
 * @brief Initialize a program (zero state).
 */
void oas_program_init(oas_program_t *prog);

/**
 * @brief Destroy a program and free its instruction array.
 */
void oas_program_destroy(oas_program_t *prog);

/**
 * @brief Emit an instruction to the program.
 * @return 0 on success, -ENOMEM on failure.
 */
[[nodiscard]] int oas_program_emit(oas_program_t *prog, const oas_instruction_t *instr);

/**
 * @brief Get current emission position (for patching jumps).
 */
size_t oas_program_pos(const oas_program_t *prog);

/**
 * @brief Patch a previously emitted instruction's jump offset.
 */
void oas_program_patch(oas_program_t *prog, size_t pos, size_t target);

/**
 * @brief Reset program to empty (keeps allocated memory).
 */
void oas_program_reset(oas_program_t *prog);

/**
 * @brief Get human-readable name for an opcode.
 */
const char *oas_opcode_name(oas_opcode_t op);

#endif /* LIBOAS_COMPILER_OAS_INSTRUCTION_H */
