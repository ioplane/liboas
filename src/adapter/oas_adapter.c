#include <liboas/oas_adapter.h>

#include <liboas/oas_compiler.h>
#include <liboas/oas_emitter.h>
#include <liboas/oas_parser.h>
#include <liboas/oas_regex.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct oas_adapter {
    oas_arena_t *arena;           /**< Owns parsed doc memory (spec-first) */
    bool owns_arena;              /**< Whether adapter allocated the arena */
    const oas_doc_t *doc;         /**< Parsed document */
    oas_compiled_doc_t *compiled; /**< Compiled document for validation */
    oas_adapter_config_t config;  /**< Effective configuration */
    char *spec_json;              /**< Cached spec JSON (if serve_spec) */
    size_t spec_json_len;         /**< Length of cached spec JSON */
};

static const char *const DEFAULT_SPEC_URL = "/openapi.json";
static const char *const DEFAULT_DOCS_URL = "/docs";

static void apply_defaults(oas_adapter_config_t *config, const oas_adapter_config_t *user)
{
    if (user) {
        *config = *user;
    } else {
        memset(config, 0, sizeof(*config));
        config->validate_requests = true;
    }
    if (!config->spec_url) {
        config->spec_url = DEFAULT_SPEC_URL;
    }
    if (!config->docs_url) {
        config->docs_url = DEFAULT_DOCS_URL;
    }
}

static oas_adapter_t *adapter_finish(oas_adapter_t *adapter, oas_error_list_t *errors)
{
    /* Compile the document */
    oas_compiler_config_t cc = {
        .regex = oas_regex_libregexp_create(),
        .format_policy = 0,
    };
    adapter->compiled = oas_doc_compile(adapter->doc, &cc, errors);
    if (!adapter->compiled) {
        oas_adapter_destroy(adapter);
        return nullptr;
    }

    /* Cache spec JSON if serving */
    if (adapter->config.serve_spec) {
        adapter->spec_json = oas_doc_emit_json(adapter->doc, nullptr, &adapter->spec_json_len);
    }

    return adapter;
}

oas_adapter_t *oas_adapter_create(const char *json, size_t len, const oas_adapter_config_t *config,
                                  oas_error_list_t *errors)
{
    if (!json || len == 0) {
        return nullptr;
    }

    oas_adapter_t *adapter = calloc(1, sizeof(*adapter));
    if (!adapter) {
        return nullptr;
    }

    apply_defaults(&adapter->config, config);

    adapter->arena = oas_arena_create(0);
    if (!adapter->arena) {
        free(adapter);
        return nullptr;
    }
    adapter->owns_arena = true;

    adapter->doc = oas_doc_parse(adapter->arena, json, len, errors);
    if (!adapter->doc) {
        oas_adapter_destroy(adapter);
        return nullptr;
    }

    return adapter_finish(adapter, errors);
}

oas_adapter_t *oas_adapter_from_doc(const oas_doc_t *doc, oas_arena_t *arena,
                                    const oas_adapter_config_t *config, oas_error_list_t *errors)
{
    if (!doc || !arena) {
        return nullptr;
    }

    oas_adapter_t *adapter = calloc(1, sizeof(*adapter));
    if (!adapter) {
        return nullptr;
    }

    apply_defaults(&adapter->config, config);
    adapter->arena = arena;
    adapter->owns_arena = false;
    adapter->doc = doc;

    return adapter_finish(adapter, errors);
}

void oas_adapter_destroy(oas_adapter_t *adapter)
{
    if (!adapter) {
        return;
    }
    oas_compiled_doc_free(adapter->compiled);
    oas_emit_free(adapter->spec_json);
    if (adapter->owns_arena) {
        oas_arena_destroy(adapter->arena);
    }
    free(adapter);
}

const oas_doc_t *oas_adapter_doc(const oas_adapter_t *adapter)
{
    if (!adapter) {
        return nullptr;
    }
    return adapter->doc;
}

const oas_adapter_config_t *oas_adapter_config(const oas_adapter_t *adapter)
{
    if (!adapter) {
        return nullptr;
    }
    return &adapter->config;
}

const char *oas_adapter_spec_json(const oas_adapter_t *adapter, size_t *out_len)
{
    if (!adapter || !adapter->spec_json) {
        if (out_len) {
            *out_len = 0;
        }
        return nullptr;
    }
    if (out_len) {
        *out_len = adapter->spec_json_len;
    }
    return adapter->spec_json;
}

int oas_adapter_validate_request(const oas_adapter_t *adapter, const oas_http_request_t *req,
                                 oas_validation_result_t *result, oas_arena_t *arena)
{
    if (!adapter || !adapter->compiled) {
        return -EINVAL;
    }
    return oas_validate_request(adapter->compiled, req, result, arena);
}

int oas_adapter_validate_response(const oas_adapter_t *adapter, const char *path,
                                  const char *method, const oas_http_response_t *resp,
                                  oas_validation_result_t *result, oas_arena_t *arena)
{
    if (!adapter || !adapter->compiled) {
        return -EINVAL;
    }
    return oas_validate_response(adapter->compiled, path, method, resp, result, arena);
}
