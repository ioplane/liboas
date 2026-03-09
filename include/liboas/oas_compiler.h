/**
 * @file oas_compiler.h
 * @brief Schema compiler — compile schema trees to validation bytecode.
 *
 * Compiles oas_schema_t trees into flat instruction arrays for efficient
 * validation by the VM. Handles pattern compilation via regex backend,
 * format validator lookup, and composition flattening.
 */

#ifndef LIBOAS_OAS_COMPILER_H
#define LIBOAS_OAS_COMPILER_H

#include <liboas/oas_alloc.h>
#include <liboas/oas_error.h>
#include <liboas/oas_regex.h>
#include <liboas/oas_schema.h>

typedef struct oas_compiled_schema oas_compiled_schema_t;
typedef struct oas_compiled_doc oas_compiled_doc_t;

typedef struct {
    oas_regex_backend_t *regex;
    uint8_t format_policy; /**< oas_format_policy_t */
} oas_compiler_config_t;

/**
 * @brief Compile a schema tree into validation bytecode.
 * @param schema  Schema to compile.
 * @param config  Compiler configuration (regex backend, format policy).
 * @param errors  Error list for compilation diagnostics.
 * @return Compiled schema, or nullptr on failure.
 */
[[nodiscard]] oas_compiled_schema_t *oas_schema_compile(const oas_schema_t *schema,
                                                        const oas_compiler_config_t *config,
                                                        oas_error_list_t *errors);

/**
 * @brief Free a compiled schema and all resources.
 */
void oas_compiled_schema_free(oas_compiled_schema_t *compiled);

#endif /* LIBOAS_OAS_COMPILER_H */
