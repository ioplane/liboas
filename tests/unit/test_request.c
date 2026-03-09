#include <liboas/oas_compiler.h>
#include <liboas/oas_doc.h>
#include <liboas/oas_error.h>
#include <liboas/oas_regex.h>
#include <liboas/oas_schema.h>
#include <liboas/oas_validator.h>

#include <string.h>
#include <unity.h>

static oas_arena_t *arena;

void setUp(void)
{
    arena = oas_arena_create(0);
}

void tearDown(void)
{
    oas_arena_destroy(arena);
    arena = nullptr;
}

/* Helper: build a minimal doc with one path and one operation */
static oas_doc_t *build_test_doc(const char *path, const char *method,
                                 oas_schema_t *req_body_schema, oas_schema_t *resp_schema)
{
    oas_doc_t *doc = oas_arena_alloc(arena, sizeof(*doc), _Alignof(oas_doc_t));
    TEST_ASSERT_NOT_NULL(doc);
    memset(doc, 0, sizeof(*doc));
    doc->openapi = "3.2.0";

    oas_info_t *info = oas_arena_alloc(arena, sizeof(*info), _Alignof(oas_info_t));
    TEST_ASSERT_NOT_NULL(info);
    memset(info, 0, sizeof(*info));
    info->title = "Test API";
    info->version = "1.0.0";
    doc->info = info;

    /* Build operation */
    oas_operation_t *op = oas_arena_alloc(arena, sizeof(*op), _Alignof(oas_operation_t));
    TEST_ASSERT_NOT_NULL(op);
    memset(op, 0, sizeof(*op));
    op->operation_id = "testOp";

    /* Request body */
    if (req_body_schema) {
        oas_request_body_t *rb = oas_arena_alloc(arena, sizeof(*rb), _Alignof(oas_request_body_t));
        TEST_ASSERT_NOT_NULL(rb);
        memset(rb, 0, sizeof(*rb));
        rb->required = true;

        oas_media_type_t *mt = oas_arena_alloc(arena, sizeof(*mt), _Alignof(oas_media_type_t));
        TEST_ASSERT_NOT_NULL(mt);
        mt->media_type_name = "application/json";
        mt->schema = req_body_schema;

        oas_media_type_entry_t *mte =
            oas_arena_alloc(arena, sizeof(*mte), _Alignof(oas_media_type_entry_t));
        TEST_ASSERT_NOT_NULL(mte);
        mte->key = "application/json";
        mte->value = mt;

        rb->content = mte;
        rb->content_count = 1;
        op->request_body = rb;
    }

    /* Response */
    if (resp_schema) {
        oas_response_t *resp = oas_arena_alloc(arena, sizeof(*resp), _Alignof(oas_response_t));
        TEST_ASSERT_NOT_NULL(resp);
        memset(resp, 0, sizeof(*resp));
        resp->description = "OK";

        oas_media_type_t *rmt = oas_arena_alloc(arena, sizeof(*rmt), _Alignof(oas_media_type_t));
        TEST_ASSERT_NOT_NULL(rmt);
        rmt->media_type_name = "application/json";
        rmt->schema = resp_schema;

        oas_media_type_entry_t *rmte =
            oas_arena_alloc(arena, sizeof(*rmte), _Alignof(oas_media_type_entry_t));
        TEST_ASSERT_NOT_NULL(rmte);
        rmte->key = "application/json";
        rmte->value = rmt;

        resp->content = rmte;
        resp->content_count = 1;

        oas_response_entry_t *re =
            oas_arena_alloc(arena, sizeof(*re), _Alignof(oas_response_entry_t));
        TEST_ASSERT_NOT_NULL(re);
        re->status_code = "200";
        re->response = resp;

        op->responses = re;
        op->responses_count = 1;
    }

    /* Path item — assign operation to correct method slot */
    oas_path_item_t *item = oas_arena_alloc(arena, sizeof(*item), _Alignof(oas_path_item_t));
    TEST_ASSERT_NOT_NULL(item);
    memset(item, 0, sizeof(*item));
    item->path = path;

    if (strcmp(method, "GET") == 0) {
        item->get = op;
    } else if (strcmp(method, "POST") == 0) {
        item->post = op;
    } else if (strcmp(method, "PUT") == 0) {
        item->put = op;
    } else if (strcmp(method, "DELETE") == 0) {
        item->delete_ = op;
    } else if (strcmp(method, "PATCH") == 0) {
        item->patch = op;
    }

    /* Path entry */
    oas_path_entry_t *pe = oas_arena_alloc(arena, sizeof(*pe), _Alignof(oas_path_entry_t));
    TEST_ASSERT_NOT_NULL(pe);
    pe->path = path;
    pe->item = item;

    doc->paths = pe;
    doc->paths_count = 1;

    return doc;
}

/* ── Test 1: compile empty doc ─────────────────────────────────────────── */

void test_doc_compile_minimal(void)
{
    oas_doc_t *doc = oas_arena_alloc(arena, sizeof(*doc), _Alignof(oas_doc_t));
    TEST_ASSERT_NOT_NULL(doc);
    memset(doc, 0, sizeof(*doc));
    doc->openapi = "3.2.0";

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 2: compile doc with schemas ──────────────────────────────────── */

void test_doc_compile_with_schemas(void)
{
    oas_schema_t *req = oas_schema_create(arena);
    req->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *resp = oas_schema_create(arena);
    resp->type_mask = OAS_TYPE_OBJECT;

    oas_doc_t *doc = build_test_doc("/pets", "POST", req, resp);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 3: compile doc with refs ─────────────────────────────────────── */

void test_doc_compile_with_refs(void)
{
    oas_schema_t *target = oas_schema_create(arena);
    target->type_mask = OAS_TYPE_STRING;

    oas_schema_t *ref_schema = oas_schema_create(arena);
    ref_schema->ref = "#/components/schemas/Name";
    ref_schema->ref_resolved = target;

    oas_doc_t *doc = build_test_doc("/items", "POST", ref_schema, nullptr);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    /* Validate that a string body passes (ref resolves to string type) */
    oas_http_request_t req = {
        .method = "POST",
        .path = "/items",
        .content_type = "application/json",
        .body = "\"hello\"",
        .body_len = 7,
    };

    oas_validation_result_t result = {0};
    int rc = oas_validate_request(cdoc, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 4: request body passes ───────────────────────────────────────── */

void test_request_body_pass(void)
{
    oas_schema_t *req_schema = oas_schema_create(arena);
    req_schema->type_mask = OAS_TYPE_OBJECT;

    const char *required[] = {"name"};
    req_schema->required = required;
    req_schema->required_count = 1;

    oas_schema_t *name_schema = oas_schema_create(arena);
    name_schema->type_mask = OAS_TYPE_STRING;
    int rc = oas_schema_add_property(arena, req_schema, "name", name_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_doc_t *doc = build_test_doc("/pets", "POST", req_schema, nullptr);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    const char *body = "{\"name\": \"Fido\"}";
    oas_http_request_t req = {
        .method = "POST",
        .path = "/pets",
        .content_type = "application/json",
        .body = body,
        .body_len = strlen(body),
    };

    oas_validation_result_t result = {0};
    rc = oas_validate_request(cdoc, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 5: request body fails ────────────────────────────────────────── */

void test_request_body_fail(void)
{
    oas_schema_t *req_schema = oas_schema_create(arena);
    req_schema->type_mask = OAS_TYPE_OBJECT;

    oas_doc_t *doc = build_test_doc("/pets", "POST", req_schema, nullptr);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    /* Send a string instead of object */
    const char *body = "\"not an object\"";
    oas_http_request_t req = {
        .method = "POST",
        .path = "/pets",
        .content_type = "application/json",
        .body = body,
        .body_len = strlen(body),
    };

    oas_validation_result_t result = {0};
    int rc = oas_validate_request(cdoc, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 6: request missing required body ─────────────────────────────── */

void test_request_missing_body(void)
{
    oas_schema_t *req_schema = oas_schema_create(arena);
    req_schema->type_mask = OAS_TYPE_OBJECT;

    oas_doc_t *doc = build_test_doc("/pets", "POST", req_schema, nullptr);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    /* No body provided */
    oas_http_request_t req = {
        .method = "POST",
        .path = "/pets",
        .content_type = nullptr,
        .body = nullptr,
        .body_len = 0,
    };

    oas_validation_result_t result = {0};
    int rc = oas_validate_request(cdoc, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 7: query param type (skipped) ────────────────────────────────── */

void test_request_query_param_type(void)
{
    TEST_IGNORE_MESSAGE("query param parsing not yet implemented");
}

/* ── Test 8: required param (skipped) ──────────────────────────────────── */

void test_request_required_param(void)
{
    TEST_IGNORE_MESSAGE("query param parsing not yet implemented");
}

/* ── Test 9: path param (skipped) ──────────────────────────────────────── */

void test_request_path_param(void)
{
    TEST_IGNORE_MESSAGE("path param extraction not yet implemented");
}

/* ── Test 10: unknown path ─────────────────────────────────────────────── */

void test_request_unknown_path(void)
{
    oas_schema_t *req_schema = oas_schema_create(arena);
    req_schema->type_mask = OAS_TYPE_OBJECT;

    oas_doc_t *doc = build_test_doc("/pets", "POST", req_schema, nullptr);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    oas_http_request_t req = {
        .method = "POST",
        .path = "/unknown",
        .content_type = "application/json",
        .body = "{}",
        .body_len = 2,
    };

    oas_validation_result_t result = {0};
    int rc = oas_validate_request(cdoc, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 11: wrong method ─────────────────────────────────────────────── */

void test_request_wrong_method(void)
{
    oas_schema_t *req_schema = oas_schema_create(arena);
    req_schema->type_mask = OAS_TYPE_OBJECT;

    oas_doc_t *doc = build_test_doc("/pets", "POST", req_schema, nullptr);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    /* Try GET on a POST-only endpoint */
    oas_http_request_t req = {
        .method = "GET",
        .path = "/pets",
        .content_type = nullptr,
        .body = nullptr,
        .body_len = 0,
    };

    oas_validation_result_t result = {0};
    int rc = oas_validate_request(cdoc, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 12: response body passes ─────────────────────────────────────── */

void test_response_body_pass(void)
{
    oas_schema_t *resp_schema = oas_schema_create(arena);
    resp_schema->type_mask = OAS_TYPE_OBJECT;

    oas_doc_t *doc = build_test_doc("/pets", "GET", nullptr, resp_schema);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    const char *body = "{\"id\": 1}";
    oas_http_response_t resp = {
        .status_code = 200,
        .content_type = "application/json",
        .body = body,
        .body_len = strlen(body),
    };

    oas_validation_result_t result = {0};
    int rc = oas_validate_response(cdoc, "/pets", "GET", &resp, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 13: response wrong status ────────────────────────────────────── */

void test_response_wrong_status(void)
{
    oas_schema_t *resp_schema = oas_schema_create(arena);
    resp_schema->type_mask = OAS_TYPE_OBJECT;

    oas_doc_t *doc = build_test_doc("/pets", "GET", nullptr, resp_schema);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    /* Response for status 404, but doc only defines 200 */
    const char *body = "{\"error\": \"not found\"}";
    oas_http_response_t resp = {
        .status_code = 404,
        .content_type = "application/json",
        .body = body,
        .body_len = strlen(body),
    };

    oas_validation_result_t result = {0};
    int rc = oas_validate_response(cdoc, "/pets", "GET", &resp, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 14: response content type mismatch ───────────────────────────── */

void test_response_content_type(void)
{
    oas_schema_t *resp_schema = oas_schema_create(arena);
    resp_schema->type_mask = OAS_TYPE_OBJECT;

    oas_doc_t *doc = build_test_doc("/pets", "GET", nullptr, resp_schema);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    /* Send XML content type, but only JSON is defined */
    const char *body = "<pets/>";
    oas_http_response_t resp = {
        .status_code = 200,
        .content_type = "application/xml",
        .body = body,
        .body_len = strlen(body),
    };

    oas_validation_result_t result = {0};
    int rc = oas_validate_response(cdoc, "/pets", "GET", &resp, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_compiled_doc_free(cdoc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_doc_compile_minimal);
    RUN_TEST(test_doc_compile_with_schemas);
    RUN_TEST(test_doc_compile_with_refs);
    RUN_TEST(test_request_body_pass);
    RUN_TEST(test_request_body_fail);
    RUN_TEST(test_request_missing_body);
    RUN_TEST(test_request_query_param_type);
    RUN_TEST(test_request_required_param);
    RUN_TEST(test_request_path_param);
    RUN_TEST(test_request_unknown_path);
    RUN_TEST(test_request_wrong_method);
    RUN_TEST(test_response_body_pass);
    RUN_TEST(test_response_wrong_status);
    RUN_TEST(test_response_content_type);
    return UNITY_END();
}
