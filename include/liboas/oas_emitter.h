/**
 * @file oas_emitter.h
 * @brief JSON emission for OpenAPI 3.2 documents and schemas.
 *
 * Converts in-memory OAS document model back to JSON using yyjson.
 */

#ifndef LIBOAS_OAS_EMITTER_H
#define LIBOAS_OAS_EMITTER_H

#include <liboas/oas_doc.h>
#include <liboas/oas_schema.h>

#include <stddef.h>

typedef struct {
    bool pretty; /**< Enable pretty-printing with indentation */
} oas_emit_options_t;

/**
 * @brief Emit an OAS document as a JSON string.
 * @param doc     Document to emit.
 * @param options Emission options (nullptr for defaults).
 * @param out_len If non-null, receives the output length.
 * @return Heap-allocated JSON string, or nullptr on failure. Free with oas_emit_free().
 */
[[nodiscard]] char *oas_doc_emit_json(const oas_doc_t *doc, const oas_emit_options_t *options,
                                      size_t *out_len);

/**
 * @brief Emit a single schema as a JSON string.
 * @param schema  Schema to emit.
 * @param options Emission options (nullptr for defaults).
 * @param out_len If non-null, receives the output length.
 * @return Heap-allocated JSON string, or nullptr on failure. Free with oas_emit_free().
 */
[[nodiscard]] char *oas_schema_emit_json(const oas_schema_t *schema,
                                         const oas_emit_options_t *options, size_t *out_len);

/**
 * @brief Free a JSON string returned by emission functions.
 * @param json String to free (nullptr-safe).
 */
void oas_emit_free(char *json);

#endif /* LIBOAS_OAS_EMITTER_H */
