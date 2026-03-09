/**
 * @file oas_regex.h
 * @brief Regex backend abstraction for JSON Schema pattern validation.
 *
 * Provides a vtable-based regex backend allowing PCRE2 (default) or
 * QuickJS libregexp (optional strict ECMA-262). Patterns are compiled
 * once and matched many times during validation.
 */

#ifndef LIBOAS_OAS_REGEX_H
#define LIBOAS_OAS_REGEX_H

#include <stdbool.h>
#include <stddef.h>

typedef struct oas_regex_backend oas_regex_backend_t;
typedef struct oas_compiled_pattern oas_compiled_pattern_t;

/**
 * @brief Regex backend vtable.
 *
 * Each backend implements compile, match, free, and destroy operations.
 * Patterns are unanchored per JSON Schema specification.
 */
struct oas_regex_backend {
    /**
     * @brief Compile a regex pattern.
     * @param backend  Backend instance.
     * @param pattern  ECMA-262 regex pattern string.
     * @param out      Receives compiled pattern on success.
     * @return 0 on success, negative errno on failure.
     */
    int (*compile)(oas_regex_backend_t *backend, const char *pattern,
                   oas_compiled_pattern_t **out);

    /**
     * @brief Match a compiled pattern against a string.
     * @param backend   Backend instance.
     * @param compiled  Previously compiled pattern.
     * @param value     String to match against.
     * @param len       Length of value in bytes.
     * @return true if pattern matches anywhere in value.
     */
    bool (*match)(oas_regex_backend_t *backend, const oas_compiled_pattern_t *compiled,
                  const char *value, size_t len);

    /**
     * @brief Free a compiled pattern.
     * @param backend   Backend instance.
     * @param compiled  Pattern to free (nullptr-safe).
     */
    void (*free_pattern)(oas_regex_backend_t *backend, oas_compiled_pattern_t *compiled);

    /**
     * @brief Destroy the backend and all resources.
     * @param backend  Backend to destroy (nullptr-safe).
     */
    void (*destroy)(oas_regex_backend_t *backend);
};

/**
 * @brief Create a PCRE2 regex backend with JIT compilation.
 * @return Backend instance, or nullptr on failure.
 */
[[nodiscard]] oas_regex_backend_t *oas_regex_pcre2_create(void);

#endif /* LIBOAS_OAS_REGEX_H */
