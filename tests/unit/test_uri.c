#include "parser/oas_uri.h"

#include <liboas/oas_alloc.h>

#include <errno.h>
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

void test_uri_parse_full_https(void)
{
    oas_uri_t uri;
    int rc = oas_uri_parse("https://example.com/api/v1/spec.json#/info", arena, &uri);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(uri.is_absolute);
    TEST_ASSERT_FALSE(uri.is_fragment_only);
    TEST_ASSERT_EQUAL_STRING("https", uri.scheme);
    TEST_ASSERT_EQUAL_STRING("example.com", uri.host);
    TEST_ASSERT_EQUAL_STRING("/api/v1/spec.json", uri.path);
    TEST_ASSERT_EQUAL_STRING("/info", uri.fragment);
    TEST_ASSERT_EQUAL_UINT16(0, uri.port);
}

void test_uri_parse_relative_path(void)
{
    oas_uri_t uri;
    int rc = oas_uri_parse("./schemas/pet.yaml", arena, &uri);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(uri.is_absolute);
    TEST_ASSERT_FALSE(uri.is_fragment_only);
    TEST_ASSERT_EQUAL_STRING("./schemas/pet.yaml", uri.path);
    TEST_ASSERT_NULL(uri.scheme);
    TEST_ASSERT_NULL(uri.host);
}

void test_uri_parse_fragment_only(void)
{
    oas_uri_t uri;
    int rc = oas_uri_parse("#/components/schemas/Pet", arena, &uri);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(uri.is_fragment_only);
    TEST_ASSERT_EQUAL_STRING("/components/schemas/Pet", uri.fragment);
    TEST_ASSERT_NULL(uri.scheme);
    TEST_ASSERT_NULL(uri.host);
    TEST_ASSERT_NULL(uri.path);
}

void test_uri_parse_file_scheme(void)
{
    oas_uri_t uri;
    int rc = oas_uri_parse("file:///path/to/schema.yaml", arena, &uri);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(uri.is_absolute);
    TEST_ASSERT_FALSE(uri.is_fragment_only);
    TEST_ASSERT_EQUAL_STRING("file", uri.scheme);
    TEST_ASSERT_EQUAL_STRING("", uri.host);
    TEST_ASSERT_EQUAL_STRING("/path/to/schema.yaml", uri.path);
    TEST_ASSERT_NULL(uri.fragment);
    TEST_ASSERT_NULL(uri.query);
}

void test_uri_parse_with_port(void)
{
    oas_uri_t uri;
    int rc = oas_uri_parse("http://localhost:8080/api/v1", arena, &uri);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(uri.is_absolute);
    TEST_ASSERT_EQUAL_STRING("http", uri.scheme);
    TEST_ASSERT_EQUAL_STRING("localhost", uri.host);
    TEST_ASSERT_EQUAL_UINT16(8080, uri.port);
    TEST_ASSERT_EQUAL_STRING("/api/v1", uri.path);
}

void test_uri_parse_null_returns_einval(void)
{
    oas_uri_t uri;
    TEST_ASSERT_EQUAL_INT(-EINVAL, oas_uri_parse(nullptr, arena, &uri));
    TEST_ASSERT_EQUAL_INT(-EINVAL, oas_uri_parse("http://x", nullptr, &uri));
    TEST_ASSERT_EQUAL_INT(-EINVAL, oas_uri_parse("http://x", arena, nullptr));
}

void test_uri_resolve_relative_against_base(void)
{
    oas_uri_t base;
    int rc = oas_uri_parse("https://example.com/api/spec.json", arena, &base);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char *resolved = nullptr;
    rc = oas_uri_resolve(&base, "schemas/pet.yaml#/Pet", arena, &resolved);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_STRING("https://example.com/api/schemas/pet.yaml#/Pet", resolved);
}

void test_uri_resolve_fragment_only(void)
{
    oas_uri_t base;
    int rc = oas_uri_parse("https://example.com/api/spec.json", arena, &base);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char *resolved = nullptr;
    rc = oas_uri_resolve(&base, "#/components/schemas/Pet", arena, &resolved);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_STRING("https://example.com/api/spec.json#/components/schemas/Pet", resolved);
}

void test_uri_path_safe_normal(void)
{
    TEST_ASSERT_TRUE(oas_uri_path_is_safe("/api/v1/spec.json"));
    TEST_ASSERT_TRUE(oas_uri_path_is_safe("schemas/pet.yaml"));
    TEST_ASSERT_TRUE(oas_uri_path_is_safe("./schemas/pet.yaml"));
    TEST_ASSERT_TRUE(oas_uri_path_is_safe("/a/b/c"));
    TEST_ASSERT_TRUE(oas_uri_path_is_safe("file.yaml"));
    TEST_ASSERT_TRUE(oas_uri_path_is_safe(nullptr));
}

void test_uri_path_safe_traversal_blocked(void)
{
    TEST_ASSERT_FALSE(oas_uri_path_is_safe("../etc/passwd"));
    TEST_ASSERT_FALSE(oas_uri_path_is_safe("/api/../../../etc/passwd"));
    TEST_ASSERT_FALSE(oas_uri_path_is_safe("schemas/../../secret"));
    TEST_ASSERT_FALSE(oas_uri_path_is_safe(".."));
}

void test_uri_parse_with_query(void)
{
    oas_uri_t uri;
    int rc = oas_uri_parse("https://example.com/path?key=value#fragment", arena, &uri);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(uri.is_absolute);
    TEST_ASSERT_EQUAL_STRING("https", uri.scheme);
    TEST_ASSERT_EQUAL_STRING("example.com", uri.host);
    TEST_ASSERT_EQUAL_STRING("/path", uri.path);
    TEST_ASSERT_EQUAL_STRING("key=value", uri.query);
    TEST_ASSERT_EQUAL_STRING("fragment", uri.fragment);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uri_parse_full_https);
    RUN_TEST(test_uri_parse_relative_path);
    RUN_TEST(test_uri_parse_fragment_only);
    RUN_TEST(test_uri_parse_file_scheme);
    RUN_TEST(test_uri_parse_with_port);
    RUN_TEST(test_uri_parse_null_returns_einval);
    RUN_TEST(test_uri_resolve_relative_against_base);
    RUN_TEST(test_uri_resolve_fragment_only);
    RUN_TEST(test_uri_path_safe_normal);
    RUN_TEST(test_uri_path_safe_traversal_blocked);
    RUN_TEST(test_uri_parse_with_query);
    return UNITY_END();
}
