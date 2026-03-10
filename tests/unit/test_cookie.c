#include "core/oas_cookie.h"

#include <liboas/oas_compiler.h>
#include <liboas/oas_doc.h>
#include <liboas/oas_error.h>
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

/* Helper: build a doc with one GET path and cookie parameters */
static oas_doc_t *build_cookie_doc(oas_parameter_t **params, size_t params_count)
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

    oas_operation_t *op = oas_arena_alloc(arena, sizeof(*op), _Alignof(oas_operation_t));
    TEST_ASSERT_NOT_NULL(op);
    memset(op, 0, sizeof(*op));
    op->operation_id = "cookieOp";
    op->parameters = params;
    op->parameters_count = params_count;

    /* Minimal 200 response so compile succeeds */
    oas_response_t *resp = oas_arena_alloc(arena, sizeof(*resp), _Alignof(oas_response_t));
    TEST_ASSERT_NOT_NULL(resp);
    memset(resp, 0, sizeof(*resp));
    resp->description = "OK";

    oas_response_entry_t *re = oas_arena_alloc(arena, sizeof(*re), _Alignof(oas_response_entry_t));
    TEST_ASSERT_NOT_NULL(re);
    re->status_code = "200";
    re->response = resp;
    op->responses = re;
    op->responses_count = 1;

    oas_path_item_t *item = oas_arena_alloc(arena, sizeof(*item), _Alignof(oas_path_item_t));
    TEST_ASSERT_NOT_NULL(item);
    memset(item, 0, sizeof(*item));
    item->path = "/test";
    item->get = op;

    oas_path_entry_t *pe = oas_arena_alloc(arena, sizeof(*pe), _Alignof(oas_path_entry_t));
    TEST_ASSERT_NOT_NULL(pe);
    pe->path = "/test";
    pe->item = item;

    doc->paths = pe;
    doc->paths_count = 1;

    return doc;
}

/* ── Test 1: parse single cookie ──────────────────────────────────────── */

void test_cookie_parse_single(void)
{
    oas_cookie_t *cookies = nullptr;
    size_t count = 0;

    int rc = oas_cookie_parse("session=abc123", arena, &cookies, &count);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_STRING("session", cookies[0].name);
    TEST_ASSERT_EQUAL_STRING("abc123", cookies[0].value);
}

/* ── Test 2: parse multiple cookies ───────────────────────────────────── */

void test_cookie_parse_multiple(void)
{
    oas_cookie_t *cookies = nullptr;
    size_t count = 0;

    int rc = oas_cookie_parse("a=1; b=2; c=3", arena, &cookies, &count);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_size_t(3, count);
    TEST_ASSERT_EQUAL_STRING("a", cookies[0].name);
    TEST_ASSERT_EQUAL_STRING("1", cookies[0].value);
    TEST_ASSERT_EQUAL_STRING("b", cookies[1].name);
    TEST_ASSERT_EQUAL_STRING("2", cookies[1].value);
    TEST_ASSERT_EQUAL_STRING("c", cookies[2].name);
    TEST_ASSERT_EQUAL_STRING("3", cookies[2].value);
}

/* ── Test 3: cookie value validated against schema ────────────────────── */

void test_cookie_validate_schema(void)
{
    /* Create a cookie parameter with string schema, minLength=3 */
    oas_schema_t *schema = oas_schema_create(arena);
    schema->type_mask = OAS_TYPE_STRING;
    schema->min_length = 3;

    oas_parameter_t *param = oas_arena_alloc(arena, sizeof(*param), _Alignof(oas_parameter_t));
    TEST_ASSERT_NOT_NULL(param);
    memset(param, 0, sizeof(*param));
    param->name = "token";
    param->in = "cookie";
    param->required = false;
    param->schema = schema;

    oas_parameter_t *params[] = {param};
    oas_doc_t *doc = build_cookie_doc(params, 1);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    /* Valid: cookie value meets minLength=3 */
    oas_http_request_t req = {
        .method = "GET",
        .path = "/test",
        .cookie_header = "token=abcdef",
    };
    oas_validation_result_t result = {0};
    int rc = oas_validate_request(cdoc, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    /* Invalid: cookie value too short (minLength=3) */
    oas_http_request_t req2 = {
        .method = "GET",
        .path = "/test",
        .cookie_header = "token=ab",
    };
    oas_validation_result_t result2 = {0};
    rc = oas_validate_request(cdoc, &req2, &result2, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result2.valid);

    oas_compiled_doc_free(cdoc);
}

/* ── Test 4: required cookie absent produces error ────────────────────── */

void test_cookie_required_missing(void)
{
    oas_schema_t *schema = oas_schema_create(arena);
    schema->type_mask = OAS_TYPE_STRING;

    oas_parameter_t *param = oas_arena_alloc(arena, sizeof(*param), _Alignof(oas_parameter_t));
    TEST_ASSERT_NOT_NULL(param);
    memset(param, 0, sizeof(*param));
    param->name = "session_id";
    param->in = "cookie";
    param->required = true;
    param->schema = schema;

    oas_parameter_t *params[] = {param};
    oas_doc_t *doc = build_cookie_doc(params, 1);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    /* No cookie header at all */
    oas_http_request_t req = {
        .method = "GET",
        .path = "/test",
        .cookie_header = nullptr,
    };
    oas_validation_result_t result = {0};
    int rc = oas_validate_request(cdoc, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_NOT_NULL(result.errors);
    TEST_ASSERT_GREATER_THAN(0, oas_error_list_count(result.errors));

    oas_compiled_doc_free(cdoc);
}

/* ── Test 5: percent-decode cookie value ──────────────────────────────── */

void test_cookie_percent_decode(void)
{
    oas_cookie_t *cookies = nullptr;
    size_t count = 0;

    int rc = oas_cookie_parse("name=hello%20world", arena, &cookies, &count);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_STRING("name", cookies[0].name);
    TEST_ASSERT_EQUAL_STRING("hello world", cookies[0].value);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cookie_parse_single);
    RUN_TEST(test_cookie_parse_multiple);
    RUN_TEST(test_cookie_validate_schema);
    RUN_TEST(test_cookie_required_missing);
    RUN_TEST(test_cookie_percent_decode);
    return UNITY_END();
}
