/**
 * @file oas_path_match.h
 * @brief Path template matcher for OpenAPI path routing.
 *
 * Matches HTTP request paths against OpenAPI path templates and extracts
 * path parameters. Prioritizes templates with more static segments.
 */

#ifndef LIBOAS_CORE_PATH_MATCH_H
#define LIBOAS_CORE_PATH_MATCH_H

#include <liboas/oas_alloc.h>

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *name;  /* parameter name (without braces) */
    const char *value; /* matched value from request path */
} oas_path_param_t;

typedef struct {
    bool matched;
    const char *template_path; /* which template matched */
    oas_path_param_t *params;  /* extracted parameters */
    size_t params_count;
} oas_path_match_result_t;

typedef struct oas_path_matcher oas_path_matcher_t;

/**
 * @brief Create a path matcher from an array of OpenAPI path templates.
 * @param arena     Arena for all internal allocations.
 * @param templates Array of path template strings (e.g., "/pets/{petId}").
 * @param count     Number of templates.
 * @return Matcher instance, or nullptr on failure or invalid input.
 */
[[nodiscard]] oas_path_matcher_t *oas_path_matcher_create(oas_arena_t *arena,
                                                          const char **templates, size_t count);

/**
 * @brief Match a request path against registered templates.
 * @param matcher      Path matcher (from oas_path_matcher_create).
 * @param request_path HTTP request path to match (e.g., "/pets/123").
 * @param result       Output: match result with extracted parameters.
 * @param arena        Arena for result allocations (params array, strings).
 * @return 0 on success, negative errno on error (-EINVAL for bad input).
 */
[[nodiscard]] int oas_path_match(const oas_path_matcher_t *matcher, const char *request_path,
                                 oas_path_match_result_t *result, oas_arena_t *arena);

#endif /* LIBOAS_CORE_PATH_MATCH_H */
