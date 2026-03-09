#include "oas_json.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int oas_json_parse(const char *data, size_t len, oas_json_doc_t *out, oas_error_list_t *errors)
{
    if (!data || !out) {
        return -EINVAL;
    }

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char *)data, len, 0, nullptr, &err);
    if (!doc) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_PARSE, "/", "JSON parse error at byte %zu: %s",
                               err.pos, err.msg);
        }
        out->doc = nullptr;
        out->root = nullptr;
        return -EINVAL;
    }

    out->doc = doc;
    out->root = yyjson_doc_get_root(doc);
    return 0;
}

int oas_json_parse_file(const char *path, oas_json_doc_t *out, oas_error_list_t *errors)
{
    if (!path || !out) {
        return -EINVAL;
    }

    long fsize = 0;
    size_t size = 0;
    size_t read_bytes = 0;
    char *buf = nullptr;
    int rc = 0;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_PARSE, "/", "cannot open file: %s (%s)", path,
                               strerror(errno));
        }
        out->doc = nullptr;
        out->root = nullptr;
        return -errno;
    }

    /* Read entire file */
    if (fseek(fp, 0, SEEK_END) != 0) {
        goto io_fail;
    }

    fsize = ftell(fp);
    if (fsize < 0) {
        goto io_fail;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        goto io_fail;
    }

    size = (size_t)fsize;
    buf = malloc(size + 1);
    if (!buf) {
        (void)fclose(fp);
        out->doc = nullptr;
        out->root = nullptr;
        return -ENOMEM;
    }

    read_bytes = fread(buf, 1, size, fp);
    (void)fclose(fp);

    if (read_bytes != size) {
        free(buf);
        out->doc = nullptr;
        out->root = nullptr;
        return -EIO;
    }
    buf[size] = '\0';

    rc = oas_json_parse(buf, size, out, errors);
    free(buf);
    return rc;

io_fail:
    (void)fclose(fp);
    out->doc = nullptr;
    out->root = nullptr;
    return -EIO;
}

void oas_json_free(oas_json_doc_t *doc)
{
    if (!doc) {
        return;
    }
    if (doc->doc) {
        yyjson_doc_free(doc->doc);
        doc->doc = nullptr;
    }
    doc->root = nullptr;
}

const char *oas_json_get_str(yyjson_val *obj, const char *key)
{
    if (!obj || !key) {
        return nullptr;
    }
    yyjson_val *val = yyjson_obj_get(obj, key);
    if (!val || !yyjson_is_str(val)) {
        return nullptr;
    }
    return yyjson_get_str(val);
}

int64_t oas_json_get_int(yyjson_val *obj, const char *key, int64_t def)
{
    if (!obj || !key) {
        return def;
    }
    yyjson_val *val = yyjson_obj_get(obj, key);
    if (!val || !yyjson_is_int(val)) {
        return def;
    }
    return yyjson_get_sint(val);
}

bool oas_json_get_bool(yyjson_val *obj, const char *key, bool def)
{
    if (!obj || !key) {
        return def;
    }
    yyjson_val *val = yyjson_obj_get(obj, key);
    if (!val || !yyjson_is_bool(val)) {
        return def;
    }
    return yyjson_get_bool(val);
}

yyjson_val *oas_json_get_obj(yyjson_val *obj, const char *key)
{
    if (!obj || !key) {
        return nullptr;
    }
    yyjson_val *val = yyjson_obj_get(obj, key);
    if (!val || !yyjson_is_obj(val)) {
        return nullptr;
    }
    return val;
}

yyjson_val *oas_json_get_arr(yyjson_val *obj, const char *key)
{
    if (!obj || !key) {
        return nullptr;
    }
    yyjson_val *val = yyjson_obj_get(obj, key);
    if (!val || !yyjson_is_arr(val)) {
        return nullptr;
    }
    return val;
}
