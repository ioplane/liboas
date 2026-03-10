#include <liboas/oas_version.h>

#include <string.h>
#include <unity.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_version_string(void)
{
    const char *ver = oas_version();
    TEST_ASSERT_NOT_NULL(ver);
    TEST_ASSERT_EQUAL_STRING("0.1.0", ver);
}

void test_version_number(void)
{
    int num = oas_version_number();
    TEST_ASSERT_EQUAL_INT(100, num);
}

void test_version_macros(void)
{
    TEST_ASSERT_EQUAL_INT(0, OAS_VERSION_MAJOR);
    TEST_ASSERT_EQUAL_INT(1, OAS_VERSION_MINOR);
    TEST_ASSERT_EQUAL_INT(0, OAS_VERSION_PATCH);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_version_string);
    RUN_TEST(test_version_number);
    RUN_TEST(test_version_macros);
    return UNITY_END();
}
