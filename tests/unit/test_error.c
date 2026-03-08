#include <liboas/oas_error.h>

#include <string.h>
#include <unity.h>

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

void test_error_list_create(void)
{
    oas_error_list_t *list = oas_error_list_create(arena);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT64(0, oas_error_list_count(list));
}

void test_error_list_add_single(void)
{
    oas_error_list_t *list = oas_error_list_create(arena);
    oas_error_list_add(list, OAS_ERR_PARSE, "/info/title", "expected string, got integer");

    TEST_ASSERT_EQUAL_UINT64(1, oas_error_list_count(list));

    const oas_error_t *err = oas_error_list_get(list, 0);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_UINT8(OAS_ERR_PARSE, err->kind);
    TEST_ASSERT_EQUAL_STRING("/info/title", err->path);
    TEST_ASSERT_EQUAL_STRING("expected string, got integer", err->message);
}

void test_error_list_add_multiple(void)
{
    oas_error_list_t *list = oas_error_list_create(arena);

    oas_error_list_add(list, OAS_ERR_REQUIRED, "/paths", "missing required field: %s", "get");
    oas_error_list_add(list, OAS_ERR_TYPE, "/info/version", "expected %s", "string");
    oas_error_list_add(list, OAS_ERR_SCHEMA, "/components", "invalid schema");

    TEST_ASSERT_EQUAL_UINT64(3, oas_error_list_count(list));
}

void test_error_list_count(void)
{
    oas_error_list_t *list = oas_error_list_create(arena);
    TEST_ASSERT_EQUAL_UINT64(0, oas_error_list_count(list));

    for (int i = 0; i < 20; i++) {
        oas_error_list_add(list, OAS_ERR_PARSE, "/", "error %d", i);
    }
    TEST_ASSERT_EQUAL_UINT64(20, oas_error_list_count(list));
}

void test_error_list_get_by_index(void)
{
    oas_error_list_t *list = oas_error_list_create(arena);

    oas_error_list_add(list, OAS_ERR_PARSE, "/a", "first");
    oas_error_list_add(list, OAS_ERR_TYPE, "/b", "second");
    oas_error_list_add(list, OAS_ERR_REF, "/c", "third");

    const oas_error_t *e0 = oas_error_list_get(list, 0);
    const oas_error_t *e1 = oas_error_list_get(list, 1);
    const oas_error_t *e2 = oas_error_list_get(list, 2);
    const oas_error_t *e3 = oas_error_list_get(list, 3);

    TEST_ASSERT_EQUAL_STRING("first", e0->message);
    TEST_ASSERT_EQUAL_STRING("second", e1->message);
    TEST_ASSERT_EQUAL_STRING("third", e2->message);
    TEST_ASSERT_NULL(e3);
}

void test_error_list_has_errors(void)
{
    oas_error_list_t *list = oas_error_list_create(arena);
    TEST_ASSERT_FALSE(oas_error_list_has_errors(list));

    oas_error_list_add(list, OAS_ERR_PARSE, "/", "something");
    TEST_ASSERT_TRUE(oas_error_list_has_errors(list));
}

void test_error_list_empty_has_no_errors(void)
{
    oas_error_list_t *list = oas_error_list_create(arena);
    TEST_ASSERT_FALSE(oas_error_list_has_errors(list));
    TEST_ASSERT_EQUAL_UINT64(0, oas_error_list_count(list));
    TEST_ASSERT_NULL(oas_error_list_get(list, 0));
}

void test_error_kind_name(void)
{
    TEST_ASSERT_EQUAL_STRING("none", oas_error_kind_name(OAS_ERR_NONE));
    TEST_ASSERT_EQUAL_STRING("parse", oas_error_kind_name(OAS_ERR_PARSE));
    TEST_ASSERT_EQUAL_STRING("schema", oas_error_kind_name(OAS_ERR_SCHEMA));
    TEST_ASSERT_EQUAL_STRING("ref", oas_error_kind_name(OAS_ERR_REF));
    TEST_ASSERT_EQUAL_STRING("type", oas_error_kind_name(OAS_ERR_TYPE));
    TEST_ASSERT_EQUAL_STRING("constraint", oas_error_kind_name(OAS_ERR_CONSTRAINT));
    TEST_ASSERT_EQUAL_STRING("required", oas_error_kind_name(OAS_ERR_REQUIRED));
    TEST_ASSERT_EQUAL_STRING("format", oas_error_kind_name(OAS_ERR_FORMAT));
    TEST_ASSERT_EQUAL_STRING("alloc", oas_error_kind_name(OAS_ERR_ALLOC));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_error_list_create);
    RUN_TEST(test_error_list_add_single);
    RUN_TEST(test_error_list_add_multiple);
    RUN_TEST(test_error_list_count);
    RUN_TEST(test_error_list_get_by_index);
    RUN_TEST(test_error_list_has_errors);
    RUN_TEST(test_error_list_empty_has_no_errors);
    RUN_TEST(test_error_kind_name);
    return UNITY_END();
}
