#include "core/oas_path_match.h"

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

void test_path_match_static_exact(void)
{
    const char *templates[] = {"/pets"};
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 1);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t result = {0};
    int rc = oas_path_match(m, "/pets", &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.matched);
    TEST_ASSERT_EQUAL_STRING("/pets", result.template_path);
    TEST_ASSERT_EQUAL_UINT64(0, result.params_count);
}

void test_path_match_single_param(void)
{
    const char *templates[] = {"/pets/{petId}"};
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 1);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t result = {0};
    int rc = oas_path_match(m, "/pets/123", &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.matched);
    TEST_ASSERT_EQUAL_UINT64(1, result.params_count);
    TEST_ASSERT_EQUAL_STRING("petId", result.params[0].name);
    TEST_ASSERT_EQUAL_STRING("123", result.params[0].value);
}

void test_path_match_multiple_params(void)
{
    const char *templates[] = {"/users/{userId}/posts/{postId}"};
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 1);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t result = {0};
    int rc = oas_path_match(m, "/users/42/posts/7", &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.matched);
    TEST_ASSERT_EQUAL_UINT64(2, result.params_count);
    TEST_ASSERT_EQUAL_STRING("userId", result.params[0].name);
    TEST_ASSERT_EQUAL_STRING("42", result.params[0].value);
    TEST_ASSERT_EQUAL_STRING("postId", result.params[1].name);
    TEST_ASSERT_EQUAL_STRING("7", result.params[1].value);
}

void test_path_match_no_match(void)
{
    const char *templates[] = {"/pets"};
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 1);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t result = {0};
    int rc = oas_path_match(m, "/users", &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.matched);
}

void test_path_match_param_extraction(void)
{
    const char *templates[] = {"/api/{version}/resources/{resourceId}"};
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 1);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t result = {0};
    int rc = oas_path_match(m, "/api/v2/resources/abc-def", &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.matched);
    TEST_ASSERT_EQUAL_UINT64(2, result.params_count);
    TEST_ASSERT_EQUAL_STRING("version", result.params[0].name);
    TEST_ASSERT_EQUAL_STRING("v2", result.params[0].value);
    TEST_ASSERT_EQUAL_STRING("resourceId", result.params[1].name);
    TEST_ASSERT_EQUAL_STRING("abc-def", result.params[1].value);
}

void test_path_match_trailing_slash(void)
{
    const char *templates[] = {"/pets"};
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 1);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t result = {0};
    int rc = oas_path_match(m, "/pets/", &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.matched);
    TEST_ASSERT_EQUAL_STRING("/pets", result.template_path);
}

void test_path_match_priority_static(void)
{
    const char *templates[] = {"/pets/{id}", "/pets/new"};
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 2);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t result = {0};
    int rc = oas_path_match(m, "/pets/new", &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.matched);
    TEST_ASSERT_EQUAL_STRING("/pets/new", result.template_path);
    TEST_ASSERT_EQUAL_UINT64(0, result.params_count);
}

void test_path_match_root(void)
{
    const char *templates[] = {"/"};
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 1);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t result = {0};
    int rc = oas_path_match(m, "/", &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.matched);
    TEST_ASSERT_EQUAL_STRING("/", result.template_path);
    TEST_ASSERT_EQUAL_UINT64(0, result.params_count);
}

void test_path_match_empty_segment(void)
{
    const char *templates[] = {"/pets/{id}"};
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 1);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t result = {0};
    int rc = oas_path_match(m, "/pets//123", &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.matched);
}

void test_path_match_multiple_templates(void)
{
    const char *templates[] = {
        "/pets",
        "/pets/{petId}",
        "/users/{userId}/pets",
    };
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 3);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t r1 = {0};
    int rc = oas_path_match(m, "/pets", &r1, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(r1.matched);
    TEST_ASSERT_EQUAL_STRING("/pets", r1.template_path);

    oas_path_match_result_t r2 = {0};
    rc = oas_path_match(m, "/pets/42", &r2, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(r2.matched);
    TEST_ASSERT_EQUAL_STRING("/pets/{petId}", r2.template_path);

    oas_path_match_result_t r3 = {0};
    rc = oas_path_match(m, "/users/99/pets", &r3, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(r3.matched);
    TEST_ASSERT_EQUAL_STRING("/users/{userId}/pets", r3.template_path);
}

void test_path_match_create_null_safe(void)
{
    TEST_ASSERT_NULL(oas_path_matcher_create(nullptr, nullptr, 0));
    TEST_ASSERT_NULL(oas_path_matcher_create(arena, nullptr, 0));

    const char *templates[] = {"/pets"};
    TEST_ASSERT_NULL(oas_path_matcher_create(arena, templates, 0));
}

void test_path_match_percent_encoded(void)
{
    const char *templates[] = {"/pets/{id}"};
    oas_path_matcher_t *m = oas_path_matcher_create(arena, templates, 1);
    TEST_ASSERT_NOT_NULL(m);

    oas_path_match_result_t result = {0};
    int rc = oas_path_match(m, "/pets/hello%20world", &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.matched);
    TEST_ASSERT_EQUAL_UINT64(1, result.params_count);
    TEST_ASSERT_EQUAL_STRING("id", result.params[0].name);
    TEST_ASSERT_EQUAL_STRING("hello%20world", result.params[0].value);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_path_match_static_exact);
    RUN_TEST(test_path_match_single_param);
    RUN_TEST(test_path_match_multiple_params);
    RUN_TEST(test_path_match_no_match);
    RUN_TEST(test_path_match_param_extraction);
    RUN_TEST(test_path_match_trailing_slash);
    RUN_TEST(test_path_match_priority_static);
    RUN_TEST(test_path_match_root);
    RUN_TEST(test_path_match_empty_segment);
    RUN_TEST(test_path_match_multiple_templates);
    RUN_TEST(test_path_match_create_null_safe);
    RUN_TEST(test_path_match_percent_encoded);
    return UNITY_END();
}
