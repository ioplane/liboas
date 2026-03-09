/**
 * @file oas_format.h
 * @brief Format validators for JSON Schema format keyword.
 *
 * Provides validation functions for standard JSON Schema formats:
 * date, date-time, time, email, uri, uri-reference, uuid, ipv4, ipv6,
 * hostname, byte (base64), int32, int64.
 */

#ifndef LIBOAS_COMPILER_OAS_FORMAT_H
#define LIBOAS_COMPILER_OAS_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum : uint8_t {
    OAS_FORMAT_IGNORE = 0,
    OAS_FORMAT_WARN,
    OAS_FORMAT_ENFORCE,
} oas_format_policy_t;

/**
 * @brief Format validation function signature.
 * @param value  String value to validate.
 * @param len    Length of value in bytes.
 * @return true if value is valid for this format.
 */
typedef bool (*oas_format_fn_t)(const char *value, size_t len);

/**
 * @brief Look up a format validator by name.
 * @param format_name  Format name (e.g. "date", "email", "uuid").
 * @return Validator function, or nullptr if format is unknown.
 */
oas_format_fn_t oas_format_get(const char *format_name);

/* Individual validators */
bool oas_format_date(const char *value, size_t len);
bool oas_format_date_time(const char *value, size_t len);
bool oas_format_time(const char *value, size_t len);
bool oas_format_email(const char *value, size_t len);
bool oas_format_uri(const char *value, size_t len);
bool oas_format_uri_reference(const char *value, size_t len);
bool oas_format_uuid(const char *value, size_t len);
bool oas_format_ipv4(const char *value, size_t len);
bool oas_format_ipv6(const char *value, size_t len);
bool oas_format_hostname(const char *value, size_t len);
bool oas_format_byte(const char *value, size_t len);
bool oas_format_int32(const char *value, size_t len);
bool oas_format_int64(const char *value, size_t len);

#endif /* LIBOAS_COMPILER_OAS_FORMAT_H */
