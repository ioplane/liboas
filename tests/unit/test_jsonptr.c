#include "core/oas_jsonptr.h"

#include <liboas/oas_alloc.h>
#include <liboas/oas_error.h>

#include <errno.h>
#include <string.h>
#include <unity.h>

/* RFC 6901 S5 example document */
static const char *RFC_JSON = "{"
                              "  \"foo\": [\"bar\", \"baz\"],"
                              "  \"\": 0,"
                              "  \"a/b\": 1,"
                              "  \"c%d\": 2,"
                              "  \"e^f\": 3,"
                              "  \"g|h\": 4,"
                              "  \"i\\\\j\": 5,"
                              "  \"k\\\"l\": 6,"
                              "  \" \": 7,"
                              "  \"m~n\": 8"
                              "}";

static yyjson_doc *jdoc;
static yyjson_val *root;
static oas_arena_t *arena;
static oas_error_list_t *errors;

void setUp(void)
{
    jdoc = yyjson_read(RFC_JSON, strlen(RFC_JSON), 0);
    root = yyjson_doc_get_root(jdoc);
    arena = oas_arena_create(0);
    errors = oas_error_list_create(arena);
}

void tearDown(void)
{
    yyjson_doc_free(jdoc);
    jdoc = nullptr;
    root = nullptr;
    oas_arena_destroy(arena);
    arena = nullptr;
    errors = nullptr;
}

void test_jsonptr_root(void)
{
    /* "" -> whole document */
    yyjson_val *val = oas_jsonptr_resolve(root, "");
    TEST_ASSERT_EQUAL_PTR(root, val);
}

void test_jsonptr_simple_key(void)
{
    /* "/foo" -> ["bar", "baz"] */
    yyjson_val *val = oas_jsonptr_resolve(root, "/foo");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_TRUE(yyjson_is_arr(val));
    TEST_ASSERT_EQUAL_UINT64(2, yyjson_arr_size(val));
}

void test_jsonptr_nested(void)
{
    /* "/foo/0" -> "bar" */
    yyjson_val *val = oas_jsonptr_resolve(root, "/foo/0");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("bar", yyjson_get_str(val));
}

void test_jsonptr_array_index(void)
{
    /* "/foo/1" -> "baz" */
    yyjson_val *val = oas_jsonptr_resolve(root, "/foo/1");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("baz", yyjson_get_str(val));
}

void test_jsonptr_tilde_escape(void)
{
    /* "/a~1b" -> key "a/b" -> value 1 */
    yyjson_val *val = oas_jsonptr_resolve(root, "/a~1b");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_TRUE(yyjson_is_int(val));
    TEST_ASSERT_EQUAL_INT(1, yyjson_get_int(val));
}

void test_jsonptr_tilde0_escape(void)
{
    /* "/m~0n" -> key "m~n" -> value 8 */
    yyjson_val *val = oas_jsonptr_resolve(root, "/m~0n");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_TRUE(yyjson_is_int(val));
    TEST_ASSERT_EQUAL_INT(8, yyjson_get_int(val));
}

void test_jsonptr_missing_key(void)
{
    yyjson_val *val = oas_jsonptr_resolve(root, "/nonexistent");
    TEST_ASSERT_NULL(val);
}

void test_jsonptr_empty_key(void)
{
    /* "/" -> key "" -> value 0 */
    yyjson_val *val = oas_jsonptr_resolve(root, "/");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_TRUE(yyjson_is_int(val));
    TEST_ASSERT_EQUAL_INT(0, yyjson_get_int(val));
}

void test_jsonptr_rfc6901_examples(void)
{
    /* All examples from RFC 6901 Section 5 */
    TEST_ASSERT_EQUAL_PTR(root, oas_jsonptr_resolve(root, ""));
    TEST_ASSERT_TRUE(yyjson_is_arr(oas_jsonptr_resolve(root, "/foo")));
    TEST_ASSERT_EQUAL_STRING("bar", yyjson_get_str(oas_jsonptr_resolve(root, "/foo/0")));
    TEST_ASSERT_EQUAL_INT(0, yyjson_get_int(oas_jsonptr_resolve(root, "/")));
    TEST_ASSERT_EQUAL_INT(1, yyjson_get_int(oas_jsonptr_resolve(root, "/a~1b")));
    TEST_ASSERT_EQUAL_INT(2, yyjson_get_int(oas_jsonptr_resolve(root, "/c%d")));
    TEST_ASSERT_EQUAL_INT(3, yyjson_get_int(oas_jsonptr_resolve(root, "/e^f")));
    TEST_ASSERT_EQUAL_INT(4, yyjson_get_int(oas_jsonptr_resolve(root, "/g|h")));
    TEST_ASSERT_EQUAL_INT(5, yyjson_get_int(oas_jsonptr_resolve(root, "/i\\j")));
    TEST_ASSERT_EQUAL_INT(6, yyjson_get_int(oas_jsonptr_resolve(root, "/k\"l")));
    TEST_ASSERT_EQUAL_INT(7, yyjson_get_int(oas_jsonptr_resolve(root, "/ ")));
    TEST_ASSERT_EQUAL_INT(8, yyjson_get_int(oas_jsonptr_resolve(root, "/m~0n")));
}

void test_jsonptr_parse_segments(void)
{
    char **segs = nullptr;
    size_t n = 0;

    /* "/foo/0" -> ["foo", "0"] */
    int rc = oas_jsonptr_parse("/foo/0", &segs, &n, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT64(2, n);
    TEST_ASSERT_EQUAL_STRING("foo", segs[0]);
    TEST_ASSERT_EQUAL_STRING("0", segs[1]);

    /* "/a~1b/m~0n" -> ["a/b", "m~n"] (unescaped) */
    rc = oas_jsonptr_parse("/a~1b/m~0n", &segs, &n, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT64(2, n);
    TEST_ASSERT_EQUAL_STRING("a/b", segs[0]);
    TEST_ASSERT_EQUAL_STRING("m~n", segs[1]);

    /* "" -> 0 segments */
    rc = oas_jsonptr_parse("", &segs, &n, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT64(0, n);
    TEST_ASSERT_NULL(segs);

    /* "/" -> [""] (empty key) */
    rc = oas_jsonptr_parse("/", &segs, &n, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT64(1, n);
    TEST_ASSERT_EQUAL_STRING("", segs[0]);
}

void test_jsonptr_from_ref(void)
{
    TEST_ASSERT_EQUAL_STRING("/components/schemas/Pet",
                             oas_jsonptr_from_ref("#/components/schemas/Pet"));
    TEST_ASSERT_EQUAL_STRING("/foo", oas_jsonptr_from_ref("/foo"));
    TEST_ASSERT_EQUAL_STRING("", oas_jsonptr_from_ref("#"));
    TEST_ASSERT_NULL(oas_jsonptr_from_ref(nullptr));
}

void test_jsonptr_resolve_ex_errors(void)
{
    /* Missing key with error reporting */
    yyjson_val *val = oas_jsonptr_resolve_ex(root, "/nonexistent", errors);
    TEST_ASSERT_NULL(val);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));

    /* nullptr root */
    oas_error_list_t *errors2 = oas_error_list_create(arena);
    val = oas_jsonptr_resolve_ex(nullptr, "/foo", errors2);
    TEST_ASSERT_NULL(val);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors2));
}

/* ── Strict validation tests (RFC 6901 S3) ────────────────────────────── */

void test_jsonptr_no_leading_slash(void)
{
    /* "foo/bar" without leading '/' must be rejected */
    char **segs = nullptr;
    size_t n = 0;
    int rc = oas_jsonptr_parse("foo/bar", &segs, &n, arena);
    TEST_ASSERT_EQUAL_INT(-EINVAL, rc);
}

void test_jsonptr_empty_string_valid(void)
{
    /* "" is a valid JSON Pointer (references whole document) */
    char **segs = nullptr;
    size_t n = 0;
    int rc = oas_jsonptr_parse("", &segs, &n, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT64(0, n);
    TEST_ASSERT_NULL(segs);
}

void test_jsonptr_root_slash_valid(void)
{
    /* "/" is valid (empty key segment) */
    char **segs = nullptr;
    size_t n = 0;
    int rc = oas_jsonptr_parse("/", &segs, &n, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT64(1, n);
    TEST_ASSERT_EQUAL_STRING("", segs[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_jsonptr_root);
    RUN_TEST(test_jsonptr_simple_key);
    RUN_TEST(test_jsonptr_nested);
    RUN_TEST(test_jsonptr_array_index);
    RUN_TEST(test_jsonptr_tilde_escape);
    RUN_TEST(test_jsonptr_tilde0_escape);
    RUN_TEST(test_jsonptr_missing_key);
    RUN_TEST(test_jsonptr_empty_key);
    RUN_TEST(test_jsonptr_rfc6901_examples);
    RUN_TEST(test_jsonptr_parse_segments);
    RUN_TEST(test_jsonptr_from_ref);
    RUN_TEST(test_jsonptr_resolve_ex_errors);
    RUN_TEST(test_jsonptr_no_leading_slash);
    RUN_TEST(test_jsonptr_empty_string_valid);
    RUN_TEST(test_jsonptr_root_slash_valid);
    return UNITY_END();
}
