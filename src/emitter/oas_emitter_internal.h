/**
 * @file oas_emitter_internal.h
 * @brief Internal helpers shared between JSON and YAML emitters.
 *
 * These functions build a yyjson_mut_doc from in-memory OAS structures.
 * They are not part of the public API.
 */

#ifndef LIBOAS_EMITTER_INTERNAL_H
#define LIBOAS_EMITTER_INTERNAL_H

#include <liboas/oas_doc.h>
#include <liboas/oas_schema.h>

#include <yyjson.h>

/**
 * @brief Build a mutable yyjson value tree from an OAS document.
 * @param doc  Mutable yyjson document (owner of allocated nodes).
 * @param oas  OAS document to convert.
 * @return Root mutable value, or nullptr on failure.
 */
yyjson_mut_val *oas_emit_build_doc(yyjson_mut_doc *doc, const oas_doc_t *oas);

/**
 * @brief Build a mutable yyjson value tree from an OAS schema.
 * @param doc    Mutable yyjson document (owner of allocated nodes).
 * @param schema OAS schema to convert.
 * @return Root mutable value, or nullptr on failure.
 */
yyjson_mut_val *oas_emit_build_schema(yyjson_mut_doc *doc, const oas_schema_t *schema);

#endif /* LIBOAS_EMITTER_INTERNAL_H */
