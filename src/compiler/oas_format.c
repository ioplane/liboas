#include "oas_format.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool parse_digits(const char *s, size_t count, int *out)
{
    int val = 0;
    for (size_t i = 0; i < count; i++) {
        if (!is_digit(s[i])) {
            return false;
        }
        val = val * 10 + (s[i] - '0');
    }
    *out = val;
    return true;
}

static bool is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_month(int year, int month)
{
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[month];
}

/* ── RFC 3339 date: YYYY-MM-DD ───────────────────────────────────────────── */

bool oas_format_date(const char *value, size_t len)
{
    if (len != 10) {
        return false;
    }
    if (value[4] != '-' || value[7] != '-') {
        return false;
    }

    int year, month, day;
    if (!parse_digits(value, 4, &year)) {
        return false;
    }
    if (!parse_digits(value + 5, 2, &month)) {
        return false;
    }
    if (!parse_digits(value + 8, 2, &day)) {
        return false;
    }

    if (month < 1 || month > 12) {
        return false;
    }
    if (day < 1 || day > days_in_month(year, month)) {
        return false;
    }

    return true;
}

/* ── RFC 3339 time: HH:MM:SS[.frac]Z or HH:MM:SS[.frac]+HH:MM ──────── */

static bool parse_time_part(const char *s, size_t len, size_t *consumed)
{
    if (len < 8) {
        return false;
    }
    if (s[2] != ':' || s[5] != ':') {
        return false;
    }

    int hour, min, sec;
    if (!parse_digits(s, 2, &hour) || hour > 23) {
        return false;
    }
    if (!parse_digits(s + 3, 2, &min) || min > 59) {
        return false;
    }
    if (!parse_digits(s + 6, 2, &sec) || sec > 60) {
        return false;
    }

    size_t pos = 8;

    /* Optional fractional seconds */
    if (pos < len && s[pos] == '.') {
        pos++;
        if (pos >= len || !is_digit(s[pos])) {
            return false;
        }
        while (pos < len && is_digit(s[pos])) {
            pos++;
        }
    }

    *consumed = pos;
    return true;
}

static bool parse_timezone(const char *s, size_t len)
{
    if (len == 0) {
        return false;
    }

    if (len == 1 && (s[0] == 'Z' || s[0] == 'z')) {
        return true;
    }

    if (len == 6 && (s[0] == '+' || s[0] == '-')) {
        if (s[3] != ':') {
            return false;
        }
        int th, tm;
        if (!parse_digits(s + 1, 2, &th) || th > 23) {
            return false;
        }
        if (!parse_digits(s + 4, 2, &tm) || tm > 59) {
            return false;
        }
        return true;
    }

    return false;
}

bool oas_format_time(const char *value, size_t len)
{
    size_t consumed = 0;
    if (!parse_time_part(value, len, &consumed)) {
        return false;
    }

    return parse_timezone(value + consumed, len - consumed);
}

/* ── RFC 3339 date-time: date "T" time ──────────────────────────────────── */

bool oas_format_date_time(const char *value, size_t len)
{
    if (len < 20) {
        return false;
    }

    /* Date part */
    if (!oas_format_date(value, 10)) {
        return false;
    }

    /* Separator */
    if (value[10] != 'T' && value[10] != 't') {
        return false;
    }

    /* Time part */
    return oas_format_time(value + 11, len - 11);
}

/* ── RFC 5321 email (simplified) ─────────────────────────────────────────── */

bool oas_format_email(const char *value, size_t len)
{
    if (len == 0) {
        return false;
    }

    const char *at = memchr(value, '@', len);
    if (!at) {
        return false;
    }

    size_t local_len = (size_t)(at - value);
    size_t domain_len = len - local_len - 1;

    if (local_len == 0 || local_len > 64) {
        return false;
    }
    if (domain_len == 0 || domain_len > 255) {
        return false;
    }

    /* Local part: printable ASCII, no spaces */
    for (size_t i = 0; i < local_len; i++) {
        char c = value[i];
        if (c <= ' ' || c >= 127) {
            return false;
        }
    }

    /* Domain part: basic hostname check */
    return oas_format_hostname(at + 1, domain_len);
}

/* ── RFC 3986 URI (simplified) ───────────────────────────────────────────── */

bool oas_format_uri(const char *value, size_t len)
{
    if (len == 0) {
        return false;
    }

    /* Must have scheme: alpha followed by alpha/digit/+/-/. then ':' */
    if (!isalpha((unsigned char)value[0])) {
        return false;
    }

    size_t i = 1;
    while (i < len && (isalnum((unsigned char)value[i]) || value[i] == '+' || value[i] == '-' ||
                       value[i] == '.')) {
        i++;
    }

    if (i >= len || value[i] != ':') {
        return false;
    }

    return true;
}

/* ── RFC 3986 URI-reference ──────────────────────────────────────────────── */

bool oas_format_uri_reference(const char *value, size_t len)
{
    if (len == 0) {
        return true;
    }

    /* A URI-reference is either a URI or a relative-reference */
    if (oas_format_uri(value, len)) {
        return true;
    }

    /* Relative reference: no control characters */
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c < 0x20 && c != '\t') {
            return false;
        }
    }
    return true;
}

/* ── RFC 4122 UUID ───────────────────────────────────────────────────────── */

bool oas_format_uuid(const char *value, size_t len)
{
    /* 8-4-4-4-12 hex with dashes = 36 chars */
    if (len != 36) {
        return false;
    }

    static const int dash_pos[] = {8, 13, 18, 23};
    for (int i = 0; i < 4; i++) {
        if (value[dash_pos[i]] != '-') {
            return false;
        }
    }

    for (size_t i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue;
        }
        if (!isxdigit((unsigned char)value[i])) {
            return false;
        }
    }

    return true;
}

/* ── IPv4 ────────────────────────────────────────────────────────────────── */

bool oas_format_ipv4(const char *value, size_t len)
{
    if (len < 7 || len > 15) {
        return false;
    }

    int octets = 0;
    int cur = 0;
    int digits = 0;

    for (size_t i = 0; i <= len; i++) {
        if (i == len || value[i] == '.') {
            if (digits == 0 || digits > 3) {
                return false;
            }
            if (cur > 255) {
                return false;
            }
            /* No leading zeros */
            if (digits > 1 && value[i - (size_t)digits] == '0') {
                return false;
            }
            octets++;
            cur = 0;
            digits = 0;
        } else if (is_digit(value[i])) {
            cur = cur * 10 + (value[i] - '0');
            digits++;
        } else {
            return false;
        }
    }

    return octets == 4;
}

/* ── IPv6 (simplified) ───────────────────────────────────────────────────── */

bool oas_format_ipv6(const char *value, size_t len)
{
    if (len < 2 || len > 45) {
        return false;
    }

    int groups = 0;
    int digits = 0;
    bool has_double_colon = false;

    for (size_t i = 0; i < len; i++) {
        if (isxdigit((unsigned char)value[i])) {
            digits++;
            if (digits > 4) {
                return false;
            }
        } else if (value[i] == ':') {
            if (i + 1 < len && value[i + 1] == ':') {
                if (has_double_colon) {
                    return false;
                }
                has_double_colon = true;
                if (digits > 0) {
                    groups++;
                }
                digits = 0;
                i++;
            } else {
                if (digits == 0 && i > 0 && value[i - 1] != ':') {
                    return false;
                }
                if (digits > 0) {
                    groups++;
                }
                digits = 0;
            }
        } else if (value[i] == '.') {
            /* IPv4-mapped suffix — must have at least one colon before the IPv4 part */
            size_t v4_start = i;
            while (v4_start > 0 && value[v4_start - 1] != ':') {
                v4_start--;
            }
            if (v4_start == 0) {
                return false; /* bare IPv4 is not valid IPv6 */
            }
            return oas_format_ipv4(value + v4_start, len - v4_start);
        } else {
            return false;
        }
    }

    if (digits > 0) {
        groups++;
    }

    if (has_double_colon) {
        return groups <= 7;
    }
    return groups == 8;
}

/* ── RFC 1123 hostname ───────────────────────────────────────────────────── */

bool oas_format_hostname(const char *value, size_t len)
{
    if (len == 0 || len > 253) {
        return false;
    }

    size_t label_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (value[i] == '.') {
            if (label_len == 0) {
                return false;
            }
            label_len = 0;
        } else if (isalnum((unsigned char)value[i]) || value[i] == '-') {
            label_len++;
            if (label_len > 63) {
                return false;
            }
        } else {
            return false;
        }
    }

    /* Last label must not be empty (trailing dot is technically valid) */
    return label_len > 0 || (len > 1 && value[len - 1] == '.');
}

/* ── Base64 (RFC 4648) ───────────────────────────────────────────────────── */

bool oas_format_byte(const char *value, size_t len)
{
    if (len == 0) {
        return true;
    }

    /* Length must be multiple of 4 */
    if (len % 4 != 0) {
        return false;
    }

    /* Find where padding starts (if any) */
    size_t data_end = len;
    for (size_t i = 0; i < len; i++) {
        if (value[i] == '=') {
            data_end = i;
            break;
        }
    }

    /* Validate data characters */
    for (size_t i = 0; i < data_end; i++) {
        unsigned char c = (unsigned char)value[i];
        if (!isalnum(c) && value[i] != '+' && value[i] != '/') {
            return false;
        }
    }

    /* Validate padding: only '=' allowed, max 2 pad chars */
    size_t pad = len - data_end;
    if (pad > 2) {
        return false;
    }
    for (size_t i = data_end; i < len; i++) {
        if (value[i] != '=') {
            return false;
        }
    }

    return true;
}

/* ── int32 / int64 ───────────────────────────────────────────────────────── */

bool oas_format_int32(const char *value, size_t len)
{
    if (len == 0) {
        return false;
    }

    char buf[32];
    if (len >= sizeof(buf)) {
        return false;
    }
    memcpy(buf, value, len);
    buf[len] = '\0';

    char *end = nullptr;
    errno = 0;
    long long val = strtoll(buf, &end, 10);
    if (errno != 0 || end != buf + len) {
        return false;
    }
    if (*end != '\0') {
        return false;
    }

    return val >= INT32_MIN && val <= INT32_MAX;
}

bool oas_format_int64(const char *value, size_t len)
{
    if (len == 0) {
        return false;
    }

    char buf[32];
    if (len >= sizeof(buf)) {
        return false;
    }
    memcpy(buf, value, len);
    buf[len] = '\0';

    char *end = nullptr;
    errno = 0;
    (void)strtoll(buf, &end, 10);
    if (errno != 0 || end != buf + len) {
        return false;
    }

    return true;
}

/* ── Lookup table ────────────────────────────────────────────────────────── */

static const struct {
    const char *name;
    oas_format_fn_t fn;
} format_table[] = {
    {"date", oas_format_date},   {"date-time", oas_format_date_time},
    {"time", oas_format_time},   {"email", oas_format_email},
    {"uri", oas_format_uri},     {"uri-reference", oas_format_uri_reference},
    {"uuid", oas_format_uuid},   {"ipv4", oas_format_ipv4},
    {"ipv6", oas_format_ipv6},   {"hostname", oas_format_hostname},
    {"byte", oas_format_byte},   {"int32", oas_format_int32},
    {"int64", oas_format_int64},
};

oas_format_fn_t oas_format_get(const char *format_name)
{
    if (!format_name) {
        return nullptr;
    }

    for (size_t i = 0; i < sizeof(format_table) / sizeof(format_table[0]); i++) {
        if (strcmp(format_table[i].name, format_name) == 0) {
            return format_table[i].fn;
        }
    }

    return nullptr;
}
