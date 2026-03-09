/**
 * @file oas_problem.h
 * @brief RFC 9457 Problem Details for HTTP APIs.
 */

#ifndef LIBOAS_OAS_PROBLEM_H
#define LIBOAS_OAS_PROBLEM_H

#include <liboas/oas_validator.h>

#include <stddef.h>

typedef struct {
    const char *type;     /**< URI identifying the problem type */
    const char *title;    /**< Short human-readable summary */
    int status;           /**< HTTP status code */
    const char *detail;   /**< Human-readable explanation */
    const char *instance; /**< URI identifying the specific occurrence */
} oas_problem_t;

/**
 * @brief Create a Problem Details JSON response from validation errors.
 * @param result      Validation result with errors.
 * @param status_code HTTP status code (e.g., 422).
 * @param out_len     Receives JSON length (nullable).
 * @return Heap-allocated JSON string. Caller must free with oas_problem_free().
 *         Returns nullptr if result is valid (no errors).
 */
[[nodiscard]] char *oas_problem_from_validation(const oas_validation_result_t *result,
                                                int status_code, size_t *out_len);

/**
 * @brief Create a Problem Details JSON response from a custom problem.
 * @param problem Problem details struct.
 * @param out_len Receives JSON length (nullable).
 * @return Heap-allocated JSON string. Caller must free with oas_problem_free().
 */
[[nodiscard]] char *oas_problem_to_json(const oas_problem_t *problem, size_t *out_len);

/**
 * @brief Free a Problem Details JSON string.
 */
void oas_problem_free(char *json);

#endif /* LIBOAS_OAS_PROBLEM_H */
