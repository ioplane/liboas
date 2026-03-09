#include <liboas/oas_problem.h>

#include <stdlib.h>

#include <yyjson.h>

char *oas_problem_from_validation(const oas_validation_result_t *result, int status_code,
                                  size_t *out_len)
{
    if (!result || result->valid) {
        return nullptr;
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    if (!doc) {
        return nullptr;
    }

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "type", "about:blank");
    yyjson_mut_obj_add_str(doc, root, "title", "Validation Failed");
    yyjson_mut_obj_add_int(doc, root, "status", status_code);
    yyjson_mut_obj_add_str(doc, root, "detail", "Request validation failed");

    if (result->errors && oas_error_list_count(result->errors) > 0) {
        yyjson_mut_val *errors_arr = yyjson_mut_arr(doc);
        size_t count = oas_error_list_count(result->errors);
        for (size_t i = 0; i < count; i++) {
            const oas_error_t *err = oas_error_list_get(result->errors, i);
            if (!err) {
                continue;
            }
            yyjson_mut_val *eobj = yyjson_mut_obj(doc);
            if (err->path) {
                yyjson_mut_obj_add_str(doc, eobj, "path", err->path);
            }
            if (err->message) {
                yyjson_mut_obj_add_str(doc, eobj, "message", err->message);
            }
            yyjson_mut_arr_append(errors_arr, eobj);
        }
        yyjson_mut_obj_add_val(doc, root, "errors", errors_arr);
    }

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (out_len) {
        *out_len = len;
    }
    yyjson_mut_doc_free(doc);
    return json;
}

char *oas_problem_to_json(const oas_problem_t *problem, size_t *out_len)
{
    if (!problem) {
        return nullptr;
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    if (!doc) {
        return nullptr;
    }

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "type", problem->type ? problem->type : "about:blank");
    if (problem->title) {
        yyjson_mut_obj_add_str(doc, root, "title", problem->title);
    }
    yyjson_mut_obj_add_int(doc, root, "status", problem->status);
    if (problem->detail) {
        yyjson_mut_obj_add_str(doc, root, "detail", problem->detail);
    }
    if (problem->instance) {
        yyjson_mut_obj_add_str(doc, root, "instance", problem->instance);
    }

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (out_len) {
        *out_len = len;
    }
    yyjson_mut_doc_free(doc);
    return json;
}

void oas_problem_free(char *json)
{
    free(json);
}
