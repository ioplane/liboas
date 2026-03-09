#include "compiler/oas_instruction.h"

#include <unity.h>

static oas_program_t prog;

void setUp(void)
{
    oas_program_init(&prog);
}

void tearDown(void)
{
    oas_program_destroy(&prog);
}

void test_instruction_program_create(void)
{
    TEST_ASSERT_NULL(prog.code);
    TEST_ASSERT_EQUAL_UINT64(0, prog.count);
    TEST_ASSERT_EQUAL_UINT64(0, prog.capacity);
}

void test_instruction_emit_single(void)
{
    oas_instruction_t instr = {.op = OAS_OP_CHECK_TYPE, .type_mask = 0x10};
    int rc = oas_program_emit(&prog, &instr);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT64(1, prog.count);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, prog.code[0].op);
    TEST_ASSERT_EQUAL_UINT8(0x10, prog.code[0].type_mask);
}

void test_instruction_emit_sequence(void)
{
    oas_instruction_t i1 = {.op = OAS_OP_CHECK_TYPE, .type_mask = 0x10};
    oas_instruction_t i2 = {.op = OAS_OP_CHECK_MIN_LEN};
    i2.operand.i64 = 5;
    oas_instruction_t i3 = {.op = OAS_OP_END};

    TEST_ASSERT_EQUAL_INT(0, oas_program_emit(&prog, &i1));
    TEST_ASSERT_EQUAL_INT(0, oas_program_emit(&prog, &i2));
    TEST_ASSERT_EQUAL_INT(0, oas_program_emit(&prog, &i3));

    TEST_ASSERT_EQUAL_UINT64(3, prog.count);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_MIN_LEN, prog.code[1].op);
    TEST_ASSERT_EQUAL_INT64(5, prog.code[1].operand.i64);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, prog.code[2].op);
}

void test_instruction_emit_grows(void)
{
    oas_instruction_t instr = {.op = OAS_OP_NOP};
    for (int i = 0; i < 100; i++) {
        int rc = oas_program_emit(&prog, &instr);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }
    TEST_ASSERT_EQUAL_UINT64(100, prog.count);
    TEST_ASSERT_TRUE(prog.capacity >= 100);
}

void test_instruction_patch_jump(void)
{
    oas_instruction_t jump = {.op = OAS_OP_JUMP};
    jump.operand.offset = 0;
    (void)oas_program_emit(&prog, &jump);

    oas_instruction_t nop = {.op = OAS_OP_NOP};
    (void)oas_program_emit(&prog, &nop);
    (void)oas_program_emit(&prog, &nop);

    size_t target = oas_program_pos(&prog);
    oas_program_patch(&prog, 0, target);

    TEST_ASSERT_EQUAL_UINT64(3, prog.code[0].operand.offset);
}

void test_instruction_pos(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, oas_program_pos(&prog));

    oas_instruction_t instr = {.op = OAS_OP_NOP};
    (void)oas_program_emit(&prog, &instr);
    TEST_ASSERT_EQUAL_UINT64(1, oas_program_pos(&prog));

    (void)oas_program_emit(&prog, &instr);
    TEST_ASSERT_EQUAL_UINT64(2, oas_program_pos(&prog));
}

void test_instruction_reset(void)
{
    oas_instruction_t instr = {.op = OAS_OP_NOP};
    (void)oas_program_emit(&prog, &instr);
    (void)oas_program_emit(&prog, &instr);
    TEST_ASSERT_EQUAL_UINT64(2, prog.count);

    oas_program_reset(&prog);
    TEST_ASSERT_EQUAL_UINT64(0, prog.count);
    TEST_ASSERT_TRUE(prog.capacity > 0);
}

void test_instruction_destroy_null_safe(void)
{
    oas_program_destroy(nullptr);
}

void test_instruction_opcode_name_all(void)
{
    TEST_ASSERT_EQUAL_STRING("NOP", oas_opcode_name(OAS_OP_NOP));
    TEST_ASSERT_EQUAL_STRING("CHECK_TYPE", oas_opcode_name(OAS_OP_CHECK_TYPE));
    TEST_ASSERT_EQUAL_STRING("END", oas_opcode_name(OAS_OP_END));
    TEST_ASSERT_EQUAL_STRING("BRANCH_ONEOF", oas_opcode_name(OAS_OP_BRANCH_ONEOF));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", oas_opcode_name(OAS_OP_COUNT_));
}

void test_instruction_type_operand(void)
{
    oas_instruction_t instr = {.op = OAS_OP_CHECK_MINIMUM};
    instr.operand.f64 = 3.14;
    (void)oas_program_emit(&prog, &instr);

    TEST_ASSERT_EQUAL_DOUBLE(3.14, prog.code[0].operand.f64);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_instruction_program_create);
    RUN_TEST(test_instruction_emit_single);
    RUN_TEST(test_instruction_emit_sequence);
    RUN_TEST(test_instruction_emit_grows);
    RUN_TEST(test_instruction_patch_jump);
    RUN_TEST(test_instruction_pos);
    RUN_TEST(test_instruction_reset);
    RUN_TEST(test_instruction_destroy_null_safe);
    RUN_TEST(test_instruction_opcode_name_all);
    RUN_TEST(test_instruction_type_operand);
    return UNITY_END();
}
