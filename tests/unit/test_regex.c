#include <liboas/oas_regex.h>

#include <string.h>
#include <unity.h>

static oas_regex_backend_t *backend;

void setUp(void)
{
    backend = oas_regex_pcre2_create();
}

void tearDown(void)
{
    if (backend) {
        backend->destroy(backend);
        backend = nullptr;
    }
}

void test_regex_pcre2_create(void)
{
    TEST_ASSERT_NOT_NULL(backend);
    TEST_ASSERT_NOT_NULL(backend->compile);
    TEST_ASSERT_NOT_NULL(backend->match);
    TEST_ASSERT_NOT_NULL(backend->free_pattern);
    TEST_ASSERT_NOT_NULL(backend->destroy);
}

void test_regex_destroy_null_safe(void)
{
    /* Destroying nullptr should not crash */
    oas_regex_backend_t *b = oas_regex_pcre2_create();
    TEST_ASSERT_NOT_NULL(b);
    b->destroy(b);
    /* Also test: free_pattern with nullptr */
    backend->free_pattern(backend, nullptr);
}

void test_regex_compile_valid(void)
{
    oas_compiled_pattern_t *pat = nullptr;
    int rc = backend->compile(backend, "^[a-z]+$", &pat);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(pat);
    backend->free_pattern(backend, pat);
}

void test_regex_compile_invalid(void)
{
    oas_compiled_pattern_t *pat = nullptr;
    int rc = backend->compile(backend, "[invalid(", &pat);
    TEST_ASSERT_TRUE(rc < 0);
    TEST_ASSERT_NULL(pat);
}

void test_regex_match_pass(void)
{
    oas_compiled_pattern_t *pat = nullptr;
    int rc = backend->compile(backend, "^hello$", &pat);
    TEST_ASSERT_EQUAL_INT(0, rc);

    bool result = backend->match(backend, pat, "hello", 5);
    TEST_ASSERT_TRUE(result);

    backend->free_pattern(backend, pat);
}

void test_regex_match_fail(void)
{
    oas_compiled_pattern_t *pat = nullptr;
    int rc = backend->compile(backend, "^hello$", &pat);
    TEST_ASSERT_EQUAL_INT(0, rc);

    bool result = backend->match(backend, pat, "world", 5);
    TEST_ASSERT_FALSE(result);

    backend->free_pattern(backend, pat);
}

void test_regex_match_unanchored(void)
{
    /* JSON Schema patterns are unanchored — partial match should succeed */
    oas_compiled_pattern_t *pat = nullptr;
    int rc = backend->compile(backend, "foo", &pat);
    TEST_ASSERT_EQUAL_INT(0, rc);

    bool result = backend->match(backend, pat, "barfoobar", 9);
    TEST_ASSERT_TRUE(result);

    backend->free_pattern(backend, pat);
}

void test_regex_match_unicode(void)
{
    /* Unicode property match: \p{L} matches any letter */
    oas_compiled_pattern_t *pat = nullptr;
    int rc = backend->compile(backend, "\\p{L}+", &pat);
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* U+00E9 = é (2-byte UTF-8: 0xC3 0xA9) */
    const char *utf8 = "caf\xc3\xa9";
    bool result = backend->match(backend, pat, utf8, strlen(utf8));
    TEST_ASSERT_TRUE(result);

    backend->free_pattern(backend, pat);
}

void test_regex_free_pattern_null_safe(void)
{
    /* Should not crash */
    backend->free_pattern(backend, nullptr);
}

void test_regex_ecma262_backslash_d(void)
{
    /* \d should match digits (ECMA-262 compatible) */
    oas_compiled_pattern_t *pat = nullptr;
    int rc = backend->compile(backend, "^\\d{3}$", &pat);
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_TRUE(backend->match(backend, pat, "123", 3));
    TEST_ASSERT_FALSE(backend->match(backend, pat, "12", 2));
    TEST_ASSERT_FALSE(backend->match(backend, pat, "abc", 3));

    backend->free_pattern(backend, pat);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_regex_pcre2_create);
    RUN_TEST(test_regex_destroy_null_safe);
    RUN_TEST(test_regex_compile_valid);
    RUN_TEST(test_regex_compile_invalid);
    RUN_TEST(test_regex_match_pass);
    RUN_TEST(test_regex_match_fail);
    RUN_TEST(test_regex_match_unanchored);
    RUN_TEST(test_regex_match_unicode);
    RUN_TEST(test_regex_free_pattern_null_safe);
    RUN_TEST(test_regex_ecma262_backslash_d);
    return UNITY_END();
}
