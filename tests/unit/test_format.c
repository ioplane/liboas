#include "compiler/oas_format.h"

#include <string.h>
#include <unity.h>

void setUp(void)
{
}
void tearDown(void)
{
}

/* ── date ────────────────────────────────────────────────────────────────── */

void test_format_date_valid(void)
{
    TEST_ASSERT_TRUE(oas_format_date("2024-01-15", 10));
    TEST_ASSERT_TRUE(oas_format_date("2000-12-31", 10));
}

void test_format_date_invalid_month(void)
{
    TEST_ASSERT_FALSE(oas_format_date("2024-13-01", 10));
    TEST_ASSERT_FALSE(oas_format_date("2024-00-01", 10));
}

void test_format_date_leap_year(void)
{
    TEST_ASSERT_TRUE(oas_format_date("2024-02-29", 10));
    TEST_ASSERT_FALSE(oas_format_date("2023-02-29", 10));
    TEST_ASSERT_TRUE(oas_format_date("2000-02-29", 10));
    TEST_ASSERT_FALSE(oas_format_date("1900-02-29", 10));
}

/* ── date-time ───────────────────────────────────────────────────────────── */

void test_format_date_time_valid(void)
{
    TEST_ASSERT_TRUE(oas_format_date_time("2024-01-15T12:30:00Z", 20));
}

void test_format_date_time_offset(void)
{
    TEST_ASSERT_TRUE(oas_format_date_time("2024-01-15T12:30:00+05:30", 25));
    TEST_ASSERT_TRUE(oas_format_date_time("2024-01-15T12:30:00-08:00", 25));
}

/* ── time ────────────────────────────────────────────────────────────────── */

void test_format_time_valid(void)
{
    TEST_ASSERT_TRUE(oas_format_time("12:30:00Z", 9));
    TEST_ASSERT_TRUE(oas_format_time("23:59:59+05:30", 14));
    TEST_ASSERT_TRUE(oas_format_time("00:00:00.123Z", 13));
}

/* ── email ───────────────────────────────────────────────────────────────── */

void test_format_email_valid(void)
{
    TEST_ASSERT_TRUE(oas_format_email("user@example.com", 16));
    TEST_ASSERT_TRUE(oas_format_email("a@b.c", 5));
}

void test_format_email_invalid(void)
{
    TEST_ASSERT_FALSE(oas_format_email("noat.example.com", 16));
    TEST_ASSERT_FALSE(oas_format_email("@example.com", 12));
    TEST_ASSERT_FALSE(oas_format_email("user@", 5));
}

/* ── uri ─────────────────────────────────────────────────────────────────── */

void test_format_uri_valid(void)
{
    const char *u = "https://example.com/path";
    TEST_ASSERT_TRUE(oas_format_uri(u, strlen(u)));
    const char *m = "mailto:user@example.com";
    TEST_ASSERT_TRUE(oas_format_uri(m, strlen(m)));
}

void test_format_uri_no_scheme(void)
{
    const char *u = "/relative/path";
    TEST_ASSERT_FALSE(oas_format_uri(u, strlen(u)));
}

/* ── uuid ────────────────────────────────────────────────────────────────── */

void test_format_uuid_valid(void)
{
    TEST_ASSERT_TRUE(oas_format_uuid("550e8400-e29b-41d4-a716-446655440000", 36));
    TEST_ASSERT_TRUE(oas_format_uuid("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", 36));
}

void test_format_uuid_invalid(void)
{
    TEST_ASSERT_FALSE(oas_format_uuid("not-a-uuid", 10));
    TEST_ASSERT_FALSE(oas_format_uuid("550e8400-e29b-41d4-a716-44665544000", 35));
    TEST_ASSERT_FALSE(oas_format_uuid("550e8400-e29b-41d4-a716-44665544000g", 36));
}

/* ── ipv4 ────────────────────────────────────────────────────────────────── */

void test_format_ipv4_valid(void)
{
    TEST_ASSERT_TRUE(oas_format_ipv4("192.168.1.1", 11));
    TEST_ASSERT_TRUE(oas_format_ipv4("0.0.0.0", 7));
    TEST_ASSERT_TRUE(oas_format_ipv4("255.255.255.255", 15));
}

void test_format_ipv4_invalid(void)
{
    TEST_ASSERT_FALSE(oas_format_ipv4("256.0.0.1", 9));
    TEST_ASSERT_FALSE(oas_format_ipv4("1.2.3", 5));
    TEST_ASSERT_FALSE(oas_format_ipv4("01.2.3.4", 8));
}

/* ── ipv6 ────────────────────────────────────────────────────────────────── */

void test_format_ipv6_valid(void)
{
    const char *full = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";
    TEST_ASSERT_TRUE(oas_format_ipv6(full, strlen(full)));
    TEST_ASSERT_TRUE(oas_format_ipv6("::1", 3));
    TEST_ASSERT_TRUE(oas_format_ipv6("::", 2));
}

void test_format_ipv6_rejects_bare_ipv4(void)
{
    /* Pure IPv4 addresses must NOT be accepted as IPv6 */
    TEST_ASSERT_FALSE(oas_format_ipv6("1.2.3.4", 7));
    TEST_ASSERT_FALSE(oas_format_ipv6("192.168.1.1", 11));
    TEST_ASSERT_FALSE(oas_format_ipv6("255.255.255.255", 15));
    TEST_ASSERT_FALSE(oas_format_ipv6("0.0.0.0", 7));
}

void test_format_ipv6_accepts_mapped_ipv4(void)
{
    /* Valid IPv4-mapped IPv6 addresses */
    const char *mapped = "::ffff:192.168.1.1";
    TEST_ASSERT_TRUE(oas_format_ipv6(mapped, strlen(mapped)));
    const char *compat = "::192.168.1.1";
    TEST_ASSERT_TRUE(oas_format_ipv6(compat, strlen(compat)));
    const char *full = "2001:db8::192.168.1.1";
    TEST_ASSERT_TRUE(oas_format_ipv6(full, strlen(full)));
    const char *nat64 = "64:ff9b::192.0.2.1";
    TEST_ASSERT_TRUE(oas_format_ipv6(nat64, strlen(nat64)));
}

/* ── hostname ────────────────────────────────────────────────────────────── */

void test_format_hostname_valid(void)
{
    const char *h = "example.com";
    TEST_ASSERT_TRUE(oas_format_hostname(h, strlen(h)));
    TEST_ASSERT_TRUE(oas_format_hostname("a", 1));
    const char *sub = "sub.example.co.uk";
    TEST_ASSERT_TRUE(oas_format_hostname(sub, strlen(sub)));
}

/* ── byte (base64) ───────────────────────────────────────────────────────── */

void test_format_byte_valid(void)
{
    TEST_ASSERT_TRUE(oas_format_byte("SGVsbG8=", 8));
    TEST_ASSERT_TRUE(oas_format_byte("YWJj", 4));
    TEST_ASSERT_TRUE(oas_format_byte("", 0));
}

/* ── int32 ───────────────────────────────────────────────────────────────── */

void test_format_int32_range(void)
{
    TEST_ASSERT_TRUE(oas_format_int32("0", 1));
    TEST_ASSERT_TRUE(oas_format_int32("2147483647", 10));
    TEST_ASSERT_TRUE(oas_format_int32("-2147483648", 11));
    TEST_ASSERT_FALSE(oas_format_int32("2147483648", 10));
    TEST_ASSERT_FALSE(oas_format_int32("abc", 3));
}

/* ── lookup ──────────────────────────────────────────────────────────────── */

void test_format_get_unknown_returns_null(void)
{
    TEST_ASSERT_NULL(oas_format_get("unknown-format"));
    TEST_ASSERT_NULL(oas_format_get(nullptr));
}

void test_format_get_known(void)
{
    TEST_ASSERT_NOT_NULL(oas_format_get("date"));
    TEST_ASSERT_NOT_NULL(oas_format_get("email"));
    TEST_ASSERT_NOT_NULL(oas_format_get("uuid"));
    TEST_ASSERT_NOT_NULL(oas_format_get("ipv4"));
    TEST_ASSERT_NOT_NULL(oas_format_get("int32"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_format_date_valid);
    RUN_TEST(test_format_date_invalid_month);
    RUN_TEST(test_format_date_leap_year);
    RUN_TEST(test_format_date_time_valid);
    RUN_TEST(test_format_date_time_offset);
    RUN_TEST(test_format_time_valid);
    RUN_TEST(test_format_email_valid);
    RUN_TEST(test_format_email_invalid);
    RUN_TEST(test_format_uri_valid);
    RUN_TEST(test_format_uri_no_scheme);
    RUN_TEST(test_format_uuid_valid);
    RUN_TEST(test_format_uuid_invalid);
    RUN_TEST(test_format_ipv4_valid);
    RUN_TEST(test_format_ipv4_invalid);
    RUN_TEST(test_format_ipv6_valid);
    RUN_TEST(test_format_ipv6_rejects_bare_ipv4);
    RUN_TEST(test_format_ipv6_accepts_mapped_ipv4);
    RUN_TEST(test_format_hostname_valid);
    RUN_TEST(test_format_byte_valid);
    RUN_TEST(test_format_int32_range);
    RUN_TEST(test_format_get_unknown_returns_null);
    RUN_TEST(test_format_get_known);
    return UNITY_END();
}
