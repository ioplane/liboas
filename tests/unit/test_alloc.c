#include <liboas/oas_alloc.h>

#include <stdint.h>
#include <unity.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_arena_create_destroy(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    TEST_ASSERT_NOT_NULL(arena);
    TEST_ASSERT_EQUAL_UINT64(0, oas_arena_used(arena));
    oas_arena_destroy(arena);
}

void test_arena_alloc_returns_aligned(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    TEST_ASSERT_NOT_NULL(arena);

    void *p = oas_arena_alloc(arena, 100, 16);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)p % 16);

    void *p2 = oas_arena_alloc(arena, 7, 32);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)p2 % 32);

    oas_arena_destroy(arena);
}

void test_arena_alloc_multiple(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    TEST_ASSERT_NOT_NULL(arena);

    void *p1 = oas_arena_alloc(arena, 64, 8);
    void *p2 = oas_arena_alloc(arena, 128, 8);
    void *p3 = oas_arena_alloc(arena, 256, 8);

    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_NULL(p3);
    TEST_ASSERT_NOT_EQUAL(p1, p2);
    TEST_ASSERT_NOT_EQUAL(p2, p3);

    oas_arena_destroy(arena);
}

void test_arena_alloc_large_object(void)
{
    oas_arena_t *arena = oas_arena_create(1024);
    TEST_ASSERT_NOT_NULL(arena);

    /* Allocate larger than block_size — should trigger a new block */
    void *p = oas_arena_alloc(arena, 2048, 8);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(oas_arena_used(arena) >= 2048);

    oas_arena_destroy(arena);
}

void test_arena_reset(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    TEST_ASSERT_NOT_NULL(arena);

    (void)oas_arena_alloc(arena, 1000, 8);
    TEST_ASSERT_TRUE(oas_arena_used(arena) > 0);

    oas_arena_reset(arena);
    TEST_ASSERT_EQUAL_UINT64(0, oas_arena_used(arena));

    /* Can still allocate after reset */
    void *p = oas_arena_alloc(arena, 500, 8);
    TEST_ASSERT_NOT_NULL(p);

    oas_arena_destroy(arena);
}

void test_arena_used_tracking(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    TEST_ASSERT_NOT_NULL(arena);

    TEST_ASSERT_EQUAL_UINT64(0, oas_arena_used(arena));

    (void)oas_arena_alloc(arena, 100, 8);
    TEST_ASSERT_EQUAL_UINT64(100, oas_arena_used(arena));

    (void)oas_arena_alloc(arena, 200, 8);
    TEST_ASSERT_EQUAL_UINT64(300, oas_arena_used(arena));

    oas_arena_destroy(arena);
}

void test_arena_null_on_zero_size(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    TEST_ASSERT_NOT_NULL(arena);

    void *p = oas_arena_alloc(arena, 0, 8);
    TEST_ASSERT_NULL(p);

    oas_arena_destroy(arena);
}

void test_arena_alignment_power_of_two(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    TEST_ASSERT_NOT_NULL(arena);

    /* Non-power-of-two alignment should return nullptr */
    void *p = oas_arena_alloc(arena, 100, 3);
    TEST_ASSERT_NULL(p);

    void *p2 = oas_arena_alloc(arena, 100, 6);
    TEST_ASSERT_NULL(p2);

    /* Power-of-two alignment should succeed */
    void *p3 = oas_arena_alloc(arena, 100, 1);
    TEST_ASSERT_NOT_NULL(p3);

    void *p4 = oas_arena_alloc(arena, 100, 64);
    TEST_ASSERT_NOT_NULL(p4);
    TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)p4 % 64);

    oas_arena_destroy(arena);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_arena_create_destroy);
    RUN_TEST(test_arena_alloc_returns_aligned);
    RUN_TEST(test_arena_alloc_multiple);
    RUN_TEST(test_arena_alloc_large_object);
    RUN_TEST(test_arena_reset);
    RUN_TEST(test_arena_used_tracking);
    RUN_TEST(test_arena_null_on_zero_size);
    RUN_TEST(test_arena_alignment_power_of_two);
    return UNITY_END();
}
