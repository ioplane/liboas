#include <liboas/oas_alloc.h>

#include <string.h>
#include <unity.h>

#include "core/oas_intern.h"

static oas_arena_t *arena;

void setUp(void)
{
    arena = oas_arena_create(0);
}

void tearDown(void)
{
    oas_arena_destroy(arena);
    arena = nullptr;
}

void test_intern_create(void)
{
    oas_intern_t *pool = oas_intern_create(arena, 0);
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_EQUAL_UINT64(0, oas_intern_count(pool));
}

void test_intern_same_string_same_pointer(void)
{
    oas_intern_t *pool = oas_intern_create(arena, 0);

    const char *a = oas_intern_get(pool, "type", 4);
    const char *b = oas_intern_get(pool, "type", 4);

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_PTR(a, b);
    TEST_ASSERT_EQUAL_STRING("type", a);
}

void test_intern_different_strings(void)
{
    oas_intern_t *pool = oas_intern_create(arena, 0);

    const char *a = oas_intern_get(pool, "type", 4);
    const char *b = oas_intern_get(pool, "properties", 10);

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_EQUAL(a, b);
    TEST_ASSERT_EQUAL_STRING("type", a);
    TEST_ASSERT_EQUAL_STRING("properties", b);
}

void test_intern_empty_string(void)
{
    oas_intern_t *pool = oas_intern_create(arena, 0);

    const char *a = oas_intern_get(pool, "", 0);
    const char *b = oas_intern_get(pool, "", 0);

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_PTR(a, b);
    TEST_ASSERT_EQUAL_STRING("", a);
}

void test_intern_count(void)
{
    oas_intern_t *pool = oas_intern_create(arena, 0);

    oas_intern_get(pool, "a", 1);
    oas_intern_get(pool, "b", 1);
    oas_intern_get(pool, "c", 1);
    oas_intern_get(pool, "a", 1); /* duplicate */

    TEST_ASSERT_EQUAL_UINT64(3, oas_intern_count(pool));
}

void test_intern_with_embedded_null(void)
{
    oas_intern_t *pool = oas_intern_create(arena, 0);

    /* "ab\0cd" — 5 bytes including the embedded null */
    const char data[] = {'a', 'b', '\0', 'c', 'd'};
    const char *a = oas_intern_get(pool, data, 5);
    const char *b = oas_intern_get(pool, data, 5);

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_PTR(a, b);
    /* memcmp since it has embedded null */
    TEST_ASSERT_EQUAL_MEMORY(data, a, 5);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_intern_create);
    RUN_TEST(test_intern_same_string_same_pointer);
    RUN_TEST(test_intern_different_strings);
    RUN_TEST(test_intern_empty_string);
    RUN_TEST(test_intern_count);
    RUN_TEST(test_intern_with_embedded_null);
    return UNITY_END();
}
