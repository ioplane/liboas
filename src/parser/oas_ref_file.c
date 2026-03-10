#include "oas_ref.h"

#include "parser/oas_ref_cache.h"
#include "parser/oas_uri.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <yyjson.h>

/** Default maximum file size: 10 MB */
constexpr size_t OAS_REF_FILE_DEFAULT_MAX_SIZE = 10ULL * 1024 * 1024;

/**
 * Build an absolute path from a relative path and base directory.
 * If path starts with '/', it is already absolute. Otherwise prepend base_dir.
 * Caller must free() the returned string.
 */
static char *build_full_path(const char *path, const char *base_dir)
{
    if (path[0] == '/') {
        return strdup(path);
    }

    /* Use cwd if base_dir is null */
    char cwd_buf[4096];
    if (!base_dir) {
        if (!getcwd(cwd_buf, sizeof(cwd_buf))) {
            return nullptr;
        }
        base_dir = cwd_buf;
    }

    size_t dir_len = strlen(base_dir);
    size_t path_len = strlen(path);

    /* +2 for potential '/' separator and null terminator */
    size_t total = dir_len + 1 + path_len + 1;
    char *full = malloc(total);
    if (!full) {
        return nullptr;
    }

    bool needs_slash = (dir_len > 0 && base_dir[dir_len - 1] != '/');
    (void)snprintf(full, total, "%s%s%s", base_dir, needs_slash ? "/" : "", path);
    return full;
}

/**
 * Check file extension to determine if YAML.
 * Returns true for .yaml/.yml, false for .json or other.
 */
static bool is_yaml_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) {
        return false;
    }
    return (strcmp(dot, ".yaml") == 0 || strcmp(dot, ".yml") == 0);
}

int oas_ref_load_file(const char *path, const char *base_dir, oas_ref_cache_t *cache,
                      size_t max_size, yyjson_val **root_out, oas_error_list_t *errors)
{
    if (!path || !cache || !root_out) {
        return -EINVAL;
    }

    *root_out = nullptr;

    if (max_size == 0) {
        max_size = OAS_REF_FILE_DEFAULT_MAX_SIZE;
    }

    /* Step 1: Build full path */
    char *full_path = build_full_path(path, base_dir);
    if (!full_path) {
        return -errno; /* ENOMEM from malloc, or EACCES/ERANGE from getcwd */
    }

    /* Step 2: Path traversal check.
     * Path safety: string-based traversal detection. The base_dir is assumed
     * to be a trusted directory. Symlink resolution is not performed to avoid
     * TOCTOU races; deployments requiring symlink protection should use
     * filesystem-level restrictions. */
    if (!oas_uri_path_is_safe(full_path)) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, path, "path traversal blocked: %s", path);
        }
        free(full_path);
        return -EINVAL;
    }

    /* Step 3: Check cache */
    yyjson_val *cached = oas_ref_cache_get(cache, full_path);
    if (cached) {
        *root_out = cached;
        free(full_path);
        return 0;
    }

    int rc = 0;
    FILE *fp = nullptr;
    char *buf = nullptr;

    /* Step 4: Check for YAML extension */
    if (is_yaml_extension(full_path)) {
#ifdef OAS_YAML
        /* TODO: YAML parsing will be added in a future task */
        rc = -ENOTSUP;
#else
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, path, "YAML support not enabled: %s", path);
        }
        rc = -ENOTSUP;
#endif
        goto cleanup;
    }

    /* Step 5: Open and read file */

    fp = fopen(full_path, "rb");
    if (!fp) {
        rc = -errno;
        if (rc == 0) {
            rc = -ENOENT;
        }
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, path, "cannot open file: %s", full_path);
        }
        goto cleanup;
    }

    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        rc = -errno;
        goto cleanup;
    }

    long file_len = ftell(fp);
    if (file_len < 0) {
        rc = -errno;
        goto cleanup;
    }

    if ((size_t)file_len > max_size) {
        if (errors) {
            oas_error_list_add(errors, OAS_ERR_REF, path, "file too large: %ld bytes (max %zu)",
                               file_len, max_size);
        }
        rc = -EFBIG;
        goto cleanup;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        rc = -errno;
        goto cleanup;
    }

    buf = malloc((size_t)file_len + 1);
    if (!buf) {
        rc = -ENOMEM;
        goto cleanup;
    }

    size_t read_len = fread(buf, 1, (size_t)file_len, fp);
    if (read_len != (size_t)file_len) {
        rc = -EIO;
        goto cleanup;
    }
    buf[read_len] = '\0';

    /* Step 6: Parse JSON */
    {
        yyjson_doc *doc = yyjson_read(buf, read_len, 0);
        if (!doc) {
            if (errors) {
                oas_error_list_add(errors, OAS_ERR_PARSE, path, "JSON parse error in file: %s",
                                   full_path);
            }
            rc = -EINVAL;
            goto cleanup;
        }

        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!root) {
            yyjson_doc_free(doc);
            rc = -EINVAL;
            goto cleanup;
        }

        /* Step 7: Store in cache (cache takes ownership of doc) */
        rc = oas_ref_cache_put(cache, full_path, doc, root);
        if (rc < 0) {
            yyjson_doc_free(doc);
            goto cleanup;
        }

        *root_out = root;
    }

cleanup:
    free(buf);
    if (fp) {
        (void)fclose(fp);
    }
    free(full_path);
    return rc;
}
