#include "oas_yaml.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>
#include <libfyaml.h>

/**
 * Recursively convert a libfyaml node to a yyjson mutable value.
 * Scalar type detection follows YAML 1.2 Core Schema rules:
 * quoted scalars are always strings; plain scalars are checked
 * for null, boolean, integer, and float patterns.
 */
static yyjson_mut_val *convert_node(yyjson_mut_doc *mdoc, struct fy_node *node)
{
    if (!node) {
        return yyjson_mut_null(mdoc);
    }

    if (fy_node_is_scalar(node)) {
        const char *str = fy_node_get_scalar0(node);
        if (!str) {
            return yyjson_mut_null(mdoc);
        }

        enum fy_node_style style = fy_node_get_style(node);
        bool is_quoted = (style == FYNS_SINGLE_QUOTED || style == FYNS_DOUBLE_QUOTED);

        if (!is_quoted) {
            /* YAML 1.2 Core Schema: null */
            if (strcmp(str, "null") == 0 || strcmp(str, "~") == 0 || str[0] == '\0') {
                return yyjson_mut_null(mdoc);
            }

            /* YAML 1.2 Core Schema: boolean */
            if (strcmp(str, "true") == 0) {
                return yyjson_mut_bool(mdoc, true);
            }
            if (strcmp(str, "false") == 0) {
                return yyjson_mut_bool(mdoc, false);
            }

            /* Integer detection */
            char *endptr;
            errno = 0;
            long long ival = strtoll(str, &endptr, 10);
            if (endptr != str && *endptr == '\0' && errno == 0) {
                return yyjson_mut_sint(mdoc, ival);
            }

            /* Float detection */
            errno = 0;
            double dval = strtod(str, &endptr);
            if (endptr != str && *endptr == '\0' && errno == 0) {
                return yyjson_mut_real(mdoc, dval);
            }
        }

        return yyjson_mut_strcpy(mdoc, str);
    }

    if (fy_node_is_sequence(node)) {
        yyjson_mut_val *arr = yyjson_mut_arr(mdoc);
        if (!arr) {
            return nullptr;
        }

        struct fy_node *item;
        void *iter = nullptr;
        while ((item = fy_node_sequence_iterate(node, &iter))) {
            yyjson_mut_val *val = convert_node(mdoc, item);
            if (!val) {
                return nullptr;
            }
            if (!yyjson_mut_arr_append(arr, val)) {
                return nullptr;
            }
        }
        return arr;
    }

    if (fy_node_is_mapping(node)) {
        yyjson_mut_val *obj = yyjson_mut_obj(mdoc);
        if (!obj) {
            return nullptr;
        }

        struct fy_node_pair *pair;
        void *iter = nullptr;
        while ((pair = fy_node_mapping_iterate(node, &iter))) {
            struct fy_node *key = fy_node_pair_key(pair);
            struct fy_node *value = fy_node_pair_value(pair);
            const char *key_str = fy_node_get_scalar0(key);
            if (!key_str) {
                continue;
            }

            yyjson_mut_val *mval = convert_node(mdoc, value);
            if (!mval) {
                return nullptr;
            }
            yyjson_mut_obj_add_val(mdoc, obj, key_str, mval);
        }
        return obj;
    }

    return yyjson_mut_null(mdoc);
}

/**
 * Set error message if buffer is provided.
 */
static void set_error(char *err_msg, size_t err_size, const char *msg)
{
    if (err_msg && err_size > 0) {
        snprintf(err_msg, err_size, "%s", msg);
    }
}

yyjson_doc *oas_yaml_to_json(const char *yaml, size_t len, char *err_msg, size_t err_size)
{
    if (!yaml || len == 0) {
        set_error(err_msg, err_size, "empty input");
        return nullptr;
    }

    struct fy_parse_cfg cfg = {
        .flags = FYPCF_RESOLVE_DOCUMENT,
    };
    struct fy_document *fyd = fy_document_build_from_string(&cfg, yaml, len);
    if (!fyd) {
        set_error(err_msg, err_size, "YAML parse error");
        return nullptr;
    }

    struct fy_node *root = fy_document_root(fyd);

    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(nullptr);
    if (!mdoc) {
        fy_document_destroy(fyd);
        set_error(err_msg, err_size, "memory allocation error");
        return nullptr;
    }

    yyjson_mut_val *mroot = convert_node(mdoc, root);
    if (!mroot) {
        yyjson_mut_doc_free(mdoc);
        fy_document_destroy(fyd);
        set_error(err_msg, err_size, "conversion error");
        return nullptr;
    }

    yyjson_mut_doc_set_root(mdoc, mroot);

    /* Serialize mutable doc to JSON string, then re-parse as immutable */
    size_t json_len = 0;
    char *json_str = yyjson_mut_write(mdoc, 0, &json_len);
    yyjson_mut_doc_free(mdoc);
    fy_document_destroy(fyd);

    if (!json_str) {
        set_error(err_msg, err_size, "JSON write error");
        return nullptr;
    }

    yyjson_doc *jdoc = yyjson_read(json_str, json_len, 0);
    free(json_str);

    if (!jdoc) {
        set_error(err_msg, err_size, "JSON re-parse error");
    }

    return jdoc;
}

yyjson_doc *oas_yaml_file_to_json(const char *filepath, char *err_msg, size_t err_size)
{
    if (!filepath) {
        set_error(err_msg, err_size, "null filepath");
        return nullptr;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        if (err_msg && err_size > 0) {
            snprintf(err_msg, err_size, "cannot open file: %s", filepath);
        }
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize < 0) {
        fclose(f);
        set_error(err_msg, err_size, "ftell error");
        return nullptr;
    }
    fseek(f, 0, SEEK_SET);

    char *buf = malloc((size_t)fsize);
    if (!buf) {
        fclose(f);
        set_error(err_msg, err_size, "memory allocation error");
        return nullptr;
    }

    size_t nread = fread(buf, 1, (size_t)fsize, f);
    fclose(f);

    yyjson_doc *doc = oas_yaml_to_json(buf, nread, err_msg, err_size);
    free(buf);
    return doc;
}

yyjson_doc *oas_auto_parse(const char *content, size_t len, char *err_msg, size_t err_size)
{
    if (!content || len == 0) {
        set_error(err_msg, err_size, "empty input");
        return nullptr;
    }

    /* Fast path: try JSON first */
    yyjson_doc *doc = yyjson_read(content, len, 0);
    if (doc) {
        return doc;
    }

    /* Fallback: parse as YAML */
    return oas_yaml_to_json(content, len, err_msg, err_size);
}
