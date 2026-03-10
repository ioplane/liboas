#include "parser/oas_ref.h"

#include <errno.h>
#include <string.h>
#include <unity.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_fetch_invalid_url(void)
{
    char *data = nullptr;
    size_t len = 0;

    /* Garbage URL (no scheme) */
    int rc = oas_ref_fetch_http("not-a-url", 100, 0, 0, &data, &len);
    TEST_ASSERT_EQUAL_INT(-EINVAL, rc);
    TEST_ASSERT_NULL(data);
    TEST_ASSERT_EQUAL_size_t(0, len);

    /* Empty host */
    rc = oas_ref_fetch_http("http:///path", 100, 0, 0, &data, &len);
    TEST_ASSERT_EQUAL_INT(-EINVAL, rc);
    TEST_ASSERT_NULL(data);
}

void test_fetch_https_unsupported(void)
{
    char *data = nullptr;
    size_t len = 0;

    int rc = oas_ref_fetch_http("https://example.com/spec.json", 100, 0, 0, &data, &len);
    TEST_ASSERT_EQUAL_INT(-ENOTSUP, rc);
    TEST_ASSERT_NULL(data);
    TEST_ASSERT_EQUAL_size_t(0, len);
}

void test_fetch_timeout(void)
{
    char *data = nullptr;
    size_t len = 0;

    /* Connect to localhost port 1 — should get ECONNREFUSED quickly */
    int rc = oas_ref_fetch_http("http://127.0.0.1:1/test", 100, 0, 0, &data, &len);
    TEST_ASSERT_TRUE(rc < 0);
    TEST_ASSERT_NULL(data);
    TEST_ASSERT_EQUAL_size_t(0, len);
}

void test_fetch_crlf_injection(void)
{
    char *data = nullptr;
    size_t len = 0;

    /* CR in host */
    int rc = oas_ref_fetch_http("http://evil.com\r\nX-Injected: true/path", 100, 0, 0, &data, &len);
    TEST_ASSERT_EQUAL_INT(-EINVAL, rc);
    TEST_ASSERT_NULL(data);

    /* LF in path */
    rc = oas_ref_fetch_http("http://evil.com/path\nX-Injected: true", 100, 0, 0, &data, &len);
    TEST_ASSERT_EQUAL_INT(-EINVAL, rc);
    TEST_ASSERT_NULL(data);
}

void test_fetch_null_args(void)
{
    char *data = nullptr;
    size_t len = 0;

    /* Null URL */
    int rc = oas_ref_fetch_http(nullptr, 0, 0, 0, &data, &len);
    TEST_ASSERT_EQUAL_INT(-EINVAL, rc);

    /* Null out_data */
    rc = oas_ref_fetch_http("http://example.com", 0, 0, 0, nullptr, &len);
    TEST_ASSERT_EQUAL_INT(-EINVAL, rc);

    /* Null out_len */
    rc = oas_ref_fetch_http("http://example.com", 0, 0, 0, &data, nullptr);
    TEST_ASSERT_EQUAL_INT(-EINVAL, rc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fetch_invalid_url);
    RUN_TEST(test_fetch_https_unsupported);
    RUN_TEST(test_fetch_timeout);
    RUN_TEST(test_fetch_crlf_injection);
    RUN_TEST(test_fetch_null_args);
    return UNITY_END();
}
