/**
 * @file oas_yaml_emitter.c
 * @brief YAML emission for OpenAPI 3.2 documents and schemas via libfyaml.
 *
 * Strategy: reuse the JSON emitter's internal functions to build a
 * yyjson_mut_doc, serialize to JSON, parse with libfyaml, then emit
 * as YAML. libfyaml handles all YAML formatting concerns.
 */

#include <liboas/oas_emitter.h>

#include "emitter/oas_emitter_internal.h"

#include <stdlib.h>
#include <string.h>

#include <yyjson.h>
#include <libfyaml.h>

/**
 * Convert a yyjson_mut_doc to a YAML string via libfyaml.
 * Returns a malloc'd string (caller frees with free/oas_emit_free).
 */
static char *mut_doc_to_yaml(yyjson_mut_doc *mdoc, bool pretty, size_t *out_len)
{
    /* Serialize mutable doc to JSON */
    size_t json_len = 0;
    char *json_str = yyjson_mut_write(mdoc, 0, &json_len);
    if (!json_str) {
        return nullptr;
    }

    /* Parse JSON with libfyaml using JSON-force mode so the parser
     * knows the input is JSON and won't preserve unnecessary quoting */
    struct fy_parse_cfg cfg = {
        .flags = FYPCF_JSON_FORCE,
    };
    struct fy_document *fyd = fy_document_build_from_string(&cfg, json_str, json_len);
    if (!fyd) {
        free(json_str);
        return nullptr;
    }

    /* Emit as YAML — block mode for pretty, flow-oneline for compact.
     * STRIP_LABELS removes YAML anchors/tags; STRIP_DOC removes
     * document markers (---/...) for cleaner output */
    enum fy_emitter_cfg_flags flags = FYECF_STRIP_LABELS | FYECF_STRIP_DOC;
    flags |= pretty ? FYECF_MODE_BLOCK : FYECF_MODE_FLOW_ONELINE;

    char *yaml_str = fy_emit_document_to_string(fyd, flags);
    fy_document_destroy(fyd);
    free(json_str);

    if (!yaml_str) {
        return nullptr;
    }

    if (out_len) {
        *out_len = strlen(yaml_str);
    }

    return yaml_str;
}

char *oas_doc_emit_yaml(const oas_doc_t *oas, const oas_emit_options_t *options, size_t *out_len)
{
    if (!oas) {
        return nullptr;
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    if (!doc) {
        return nullptr;
    }

    yyjson_mut_val *root = oas_emit_build_doc(doc, oas);
    if (!root) {
        yyjson_mut_doc_free(doc);
        return nullptr;
    }

    yyjson_mut_doc_set_root(doc, root);

    bool pretty = options ? options->pretty : true;
    char *yaml = mut_doc_to_yaml(doc, pretty, out_len);

    yyjson_mut_doc_free(doc);
    return yaml;
}

char *oas_schema_emit_yaml(const oas_schema_t *schema, const oas_emit_options_t *options,
                           size_t *out_len)
{
    if (!schema) {
        return nullptr;
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    if (!doc) {
        return nullptr;
    }

    yyjson_mut_val *root = oas_emit_build_schema(doc, schema);
    if (!root) {
        yyjson_mut_doc_free(doc);
        return nullptr;
    }

    yyjson_mut_doc_set_root(doc, root);

    bool pretty = options ? options->pretty : true;
    char *yaml = mut_doc_to_yaml(doc, pretty, out_len);

    yyjson_mut_doc_free(doc);
    return yaml;
}
