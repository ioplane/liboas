#include <liboas/oas_adapter.h>
#include <liboas/oas_negotiate.h>
#include <liboas/oas_problem.h>

#include "core/oas_query.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>
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

/* ── Task 13.1: Header/query maps in request types ─────────────────────── */

void test_request_with_headers(void)
{
    oas_http_header_t headers[] = {
        {.name = "X-Request-ID", .value = "abc-123"},
        {.name = "Authorization", .value = "Bearer token"},
    };
    oas_http_request_t req = {
        .method = "GET",
        .path = "/pets",
        .headers = headers,
        .headers_count = 2,
    };
    TEST_ASSERT_EQUAL_STRING("X-Request-ID", req.headers[0].name);
    TEST_ASSERT_EQUAL_STRING("abc-123", req.headers[0].value);
    TEST_ASSERT_EQUAL(2, req.headers_count);
}

void test_request_without_headers(void)
{
    oas_http_request_t req = {
        .method = "GET",
        .path = "/pets",
    };
    TEST_ASSERT_NULL(req.headers);
    TEST_ASSERT_EQUAL(0, req.headers_count);
    TEST_ASSERT_NULL(req.query);
    TEST_ASSERT_EQUAL(0, req.query_count);
}

void test_response_with_headers(void)
{
    oas_http_header_t headers[] = {
        {.name = "Content-Type", .value = "application/json"},
    };
    oas_http_response_t resp = {
        .status_code = 200,
        .headers = headers,
        .headers_count = 1,
    };
    TEST_ASSERT_EQUAL_STRING("Content-Type", resp.headers[0].name);
    TEST_ASSERT_EQUAL(1, resp.headers_count);
}

/* ── Task 13.3: Query string parser ─────────────────────────────────── */

void test_query_parse_simple(void)
{
    oas_query_pair_t *pairs = nullptr;
    size_t count = 0;
    int rc = oas_query_parse(arena, "a=1&b=2", &pairs, &count);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL_STRING("a", pairs[0].key);
    TEST_ASSERT_EQUAL_STRING("1", pairs[0].value);
    TEST_ASSERT_EQUAL_STRING("b", pairs[1].key);
    TEST_ASSERT_EQUAL_STRING("2", pairs[1].value);
}

void test_query_parse_empty(void)
{
    oas_query_pair_t *pairs = nullptr;
    size_t count = 0;
    int rc = oas_query_parse(arena, "", &pairs, &count);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL(0, count);
}

void test_query_parse_encoded(void)
{
    oas_query_pair_t *pairs = nullptr;
    size_t count = 0;
    int rc = oas_query_parse(arena, "name=hello%20world&city=New+York", &pairs, &count);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL_STRING("hello world", pairs[0].value);
    TEST_ASSERT_EQUAL_STRING("New York", pairs[1].value);
}

void test_query_parse_no_value(void)
{
    oas_query_pair_t *pairs = nullptr;
    size_t count = 0;
    int rc = oas_query_parse(arena, "flag&key=val", &pairs, &count);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL_STRING("flag", pairs[0].key);
    TEST_ASSERT_EQUAL_STRING("", pairs[0].value);
    TEST_ASSERT_EQUAL_STRING("key", pairs[1].key);
    TEST_ASSERT_EQUAL_STRING("val", pairs[1].value);
}

void test_query_parse_null(void)
{
    oas_query_pair_t *pairs = nullptr;
    size_t count = 0;
    int rc = oas_query_parse(arena, nullptr, &pairs, &count);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL(0, count);
}

/* ── Task 13.5: RFC 9457 Problem Details ────────────────────────────── */

void test_problem_from_validation(void)
{
    oas_error_list_t *errors = oas_error_list_create(arena);
    oas_error_list_add(errors, OAS_ERR_SCHEMA, "/name", "required property missing");
    oas_error_list_add(errors, OAS_ERR_SCHEMA, "/age", "expected integer, got string");

    oas_validation_result_t result = {
        .valid = false,
        .errors = errors,
    };

    size_t len = 0;
    char *json = oas_problem_from_validation(&result, 422, &len);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_GREATER_THAN(0, len);

    /* Parse and verify */
    yyjson_doc *doc = yyjson_read(json, len, 0);
    TEST_ASSERT_NOT_NULL(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    TEST_ASSERT_EQUAL_STRING("about:blank", yyjson_get_str(yyjson_obj_get(root, "type")));
    TEST_ASSERT_EQUAL_STRING("Validation Failed", yyjson_get_str(yyjson_obj_get(root, "title")));
    TEST_ASSERT_EQUAL_INT(422, yyjson_get_int(yyjson_obj_get(root, "status")));

    yyjson_val *errs = yyjson_obj_get(root, "errors");
    TEST_ASSERT_NOT_NULL(errs);
    TEST_ASSERT_EQUAL(2, yyjson_arr_size(errs));

    yyjson_doc_free(doc);
    oas_problem_free(json);
}

void test_problem_custom(void)
{
    oas_problem_t problem = {
        .type = "https://example.com/not-found",
        .title = "Not Found",
        .status = 404,
        .detail = "The requested pet was not found",
        .instance = "/pets/999",
    };

    size_t len = 0;
    char *json = oas_problem_to_json(&problem, &len);
    TEST_ASSERT_NOT_NULL(json);

    yyjson_doc *doc = yyjson_read(json, len, 0);
    TEST_ASSERT_NOT_NULL(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    TEST_ASSERT_EQUAL_STRING("https://example.com/not-found",
                             yyjson_get_str(yyjson_obj_get(root, "type")));
    TEST_ASSERT_EQUAL_INT(404, yyjson_get_int(yyjson_obj_get(root, "status")));
    TEST_ASSERT_EQUAL_STRING("/pets/999", yyjson_get_str(yyjson_obj_get(root, "instance")));

    yyjson_doc_free(doc);
    oas_problem_free(json);
}

void test_problem_empty_errors(void)
{
    oas_validation_result_t result = {
        .valid = true,
        .errors = nullptr,
    };
    char *json = oas_problem_from_validation(&result, 422, nullptr);
    TEST_ASSERT_NULL(json);
}

void test_problem_roundtrip(void)
{
    oas_problem_t problem = {
        .type = "about:blank",
        .title = "Bad Request",
        .status = 400,
        .detail = "Invalid input",
    };

    size_t len = 0;
    char *json = oas_problem_to_json(&problem, &len);
    TEST_ASSERT_NOT_NULL(json);

    /* Parse back and check all fields */
    yyjson_doc *doc = yyjson_read(json, len, 0);
    TEST_ASSERT_NOT_NULL(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    TEST_ASSERT_EQUAL_STRING("about:blank", yyjson_get_str(yyjson_obj_get(root, "type")));
    TEST_ASSERT_EQUAL_STRING("Bad Request", yyjson_get_str(yyjson_obj_get(root, "title")));
    TEST_ASSERT_EQUAL_INT(400, yyjson_get_int(yyjson_obj_get(root, "status")));
    TEST_ASSERT_EQUAL_STRING("Invalid input", yyjson_get_str(yyjson_obj_get(root, "detail")));
    /* instance should be absent */
    TEST_ASSERT_NULL(yyjson_obj_get(root, "instance"));

    yyjson_doc_free(doc);
    oas_problem_free(json);
}

/* ── Task 13.7: Content Negotiation ──────────────────────────────────── */

void test_negotiate_exact_match(void)
{
    const char *available[] = {"application/json", "text/html"};
    const char *result = oas_negotiate_content_type("application/json", available, 2);
    TEST_ASSERT_EQUAL_STRING("application/json", result);
}

void test_negotiate_quality_factor(void)
{
    const char *available[] = {"text/html", "application/json"};
    const char *result =
        oas_negotiate_content_type("text/html;q=0.9, application/json;q=1.0", available, 2);
    TEST_ASSERT_EQUAL_STRING("application/json", result);
}

void test_negotiate_wildcard(void)
{
    const char *available[] = {"application/json"};
    const char *result = oas_negotiate_content_type("*/*", available, 1);
    TEST_ASSERT_EQUAL_STRING("application/json", result);
}

void test_negotiate_partial_wildcard(void)
{
    const char *available[] = {"application/json", "text/html"};
    const char *result = oas_negotiate_content_type("application/*", available, 2);
    TEST_ASSERT_EQUAL_STRING("application/json", result);
}

void test_negotiate_no_match(void)
{
    const char *available[] = {"application/json"};
    const char *result = oas_negotiate_content_type("text/xml", available, 1);
    TEST_ASSERT_NULL(result);
}

/* ── Task 13.6: Operation Lookup ─────────────────────────────────────── */

static const char PETSTORE_JSON[] =
    "{"
    "  \"openapi\": \"3.1.0\","
    "  \"info\": {\"title\": \"Petstore\", \"version\": \"1.0\"},"
    "  \"paths\": {"
    "    \"/pets\": {"
    "      \"get\": {"
    "        \"operationId\": \"listPets\","
    "        \"responses\": {\"200\": {\"description\": \"ok\"}}"
    "      },"
    "      \"post\": {"
    "        \"operationId\": \"createPet\","
    "        \"responses\": {\"201\": {\"description\": \"created\"}}"
    "      }"
    "    },"
    "    \"/pets/{petId}\": {"
    "      \"get\": {"
    "        \"operationId\": \"getPet\","
    "        \"parameters\": [{"
    "          \"name\": \"petId\", \"in\": \"path\", \"required\": true,"
    "          \"schema\": {\"type\": \"string\"}"
    "        }],"
    "        \"responses\": {\"200\": {\"description\": \"ok\"}}"
    "      }"
    "    }"
    "  }"
    "}";

void test_find_operation_exact(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PETSTORE_JSON, strlen(PETSTORE_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_matched_operation_t match = {0};
    int rc = oas_adapter_find_operation(adapter, "GET", "/pets", &match, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("listPets", match.operation_id);
    TEST_ASSERT_EQUAL_STRING("/pets", match.path_template);
    TEST_ASSERT_EQUAL(0, match.param_count);

    oas_adapter_destroy(adapter);
}

void test_find_operation_with_params(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PETSTORE_JSON, strlen(PETSTORE_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_matched_operation_t match = {0};
    int rc = oas_adapter_find_operation(adapter, "GET", "/pets/123", &match, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("getPet", match.operation_id);
    TEST_ASSERT_EQUAL_STRING("/pets/{petId}", match.path_template);
    TEST_ASSERT_EQUAL(1, match.param_count);
    TEST_ASSERT_EQUAL_STRING("petId", match.param_names[0]);
    TEST_ASSERT_EQUAL_STRING("123", match.param_values[0]);

    oas_adapter_destroy(adapter);
}

void test_find_operation_not_found(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PETSTORE_JSON, strlen(PETSTORE_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_matched_operation_t match = {0};
    int rc = oas_adapter_find_operation(adapter, "GET", "/unknown", &match, arena);
    TEST_ASSERT_EQUAL_INT(-ENOENT, rc);

    oas_adapter_destroy(adapter);
}

void test_find_operation_wrong_method(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PETSTORE_JSON, strlen(PETSTORE_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_matched_operation_t match = {0};
    int rc = oas_adapter_find_operation(adapter, "DELETE", "/pets", &match, arena);
    TEST_ASSERT_EQUAL_INT(-ENOENT, rc);

    oas_adapter_destroy(adapter);
}

/* ── Task 13.2/13.4: Header and path param validation (integration) ── */

static const char PARAM_SPEC_JSON[] =
    "{"
    "  \"openapi\": \"3.1.0\","
    "  \"info\": {\"title\": \"Test\", \"version\": \"1.0\"},"
    "  \"paths\": {"
    "    \"/items\": {"
    "      \"get\": {"
    "        \"operationId\": \"listItems\","
    "        \"parameters\": ["
    "          {\"name\": \"X-Request-ID\", \"in\": \"header\", \"required\": true,"
    "           \"schema\": {\"type\": \"string\"}},"
    "          {\"name\": \"page\", \"in\": \"query\", \"required\": false,"
    "           \"schema\": {\"type\": \"integer\"}},"
    "          {\"name\": \"limit\", \"in\": \"query\", \"required\": true,"
    "           \"schema\": {\"type\": \"integer\"}}"
    "        ],"
    "        \"responses\": {\"200\": {\"description\": \"ok\"}}"
    "      }"
    "    },"
    "    \"/items/{itemId}\": {"
    "      \"get\": {"
    "        \"operationId\": \"getItem\","
    "        \"parameters\": ["
    "          {\"name\": \"itemId\", \"in\": \"path\", \"required\": true,"
    "           \"schema\": {\"type\": \"integer\"}}"
    "        ],"
    "        \"responses\": {\"200\": {\"description\": \"ok\"}}"
    "      }"
    "    }"
    "  }"
    "}";

void test_validate_header_pass(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PARAM_SPEC_JSON, strlen(PARAM_SPEC_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_http_header_t headers[] = {{.name = "X-Request-ID", .value = "abc-123"}};
    oas_http_query_param_t query[] = {{.name = "limit", .value = "10"}};
    oas_http_request_t req = {
        .method = "GET",
        .path = "/items",
        .headers = headers,
        .headers_count = 1,
        .query = query,
        .query_count = 1,
    };

    oas_validation_result_t result = {0};
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_adapter_destroy(adapter);
}

void test_validate_header_required_missing(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PARAM_SPEC_JSON, strlen(PARAM_SPEC_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    /* Missing X-Request-ID header but providing required query param */
    oas_http_query_param_t query[] = {{.name = "limit", .value = "10"}};
    oas_http_request_t req = {
        .method = "GET",
        .path = "/items",
        .query = query,
        .query_count = 1,
    };

    oas_validation_result_t result = {0};
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_adapter_destroy(adapter);
}

void test_validate_header_optional_missing(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PARAM_SPEC_JSON, strlen(PARAM_SPEC_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    /* Provide required header and required query, skip optional 'page' query */
    oas_http_header_t headers[] = {{.name = "X-Request-ID", .value = "xyz"}};
    oas_http_query_param_t query[] = {{.name = "limit", .value = "10"}};
    oas_http_request_t req = {
        .method = "GET",
        .path = "/items",
        .headers = headers,
        .headers_count = 1,
        .query = query,
        .query_count = 1,
    };

    oas_validation_result_t result = {0};
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_adapter_destroy(adapter);
}

void test_validate_header_case_insensitive(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PARAM_SPEC_JSON, strlen(PARAM_SPEC_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    /* Lowercase header name should match */
    oas_http_header_t headers[] = {{.name = "x-request-id", .value = "test"}};
    oas_http_query_param_t query[] = {{.name = "limit", .value = "10"}};
    oas_http_request_t req = {
        .method = "GET",
        .path = "/items",
        .headers = headers,
        .headers_count = 1,
        .query = query,
        .query_count = 1,
    };

    oas_validation_result_t result = {0};
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_adapter_destroy(adapter);
}

void test_validate_query_param_pass(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PARAM_SPEC_JSON, strlen(PARAM_SPEC_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_http_header_t headers[] = {{.name = "X-Request-ID", .value = "test"}};
    oas_http_request_t req = {
        .method = "GET",
        .path = "/items",
        .headers = headers,
        .headers_count = 1,
        .query_string = "limit=10&page=2",
    };

    oas_validation_result_t result = {0};
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_adapter_destroy(adapter);
}

void test_validate_query_required_missing(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PARAM_SPEC_JSON, strlen(PARAM_SPEC_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    /* Missing required 'limit' query param */
    oas_http_header_t headers[] = {{.name = "X-Request-ID", .value = "test"}};
    oas_http_request_t req = {
        .method = "GET",
        .path = "/items",
        .headers = headers,
        .headers_count = 1,
        .query_string = "page=1",
    };

    oas_validation_result_t result = {0};
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_adapter_destroy(adapter);
}

void test_path_param_integer_pass(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PARAM_SPEC_JSON, strlen(PARAM_SPEC_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_http_request_t req = {
        .method = "GET",
        .path = "/items/123",
    };

    oas_validation_result_t result = {0};
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_adapter_destroy(adapter);
}

void test_path_param_integer_fail(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(PARAM_SPEC_JSON, strlen(PARAM_SPEC_JSON), nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_http_request_t req = {
        .method = "GET",
        .path = "/items/abc",
    };

    oas_validation_result_t result = {0};
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_adapter_destroy(adapter);
}

/* ── Main ──────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* 13.1: Header/query maps */
    RUN_TEST(test_request_with_headers);
    RUN_TEST(test_request_without_headers);
    RUN_TEST(test_response_with_headers);

    /* 13.3: Query parser */
    RUN_TEST(test_query_parse_simple);
    RUN_TEST(test_query_parse_empty);
    RUN_TEST(test_query_parse_encoded);
    RUN_TEST(test_query_parse_no_value);
    RUN_TEST(test_query_parse_null);

    /* 13.5: Problem Details */
    RUN_TEST(test_problem_from_validation);
    RUN_TEST(test_problem_custom);
    RUN_TEST(test_problem_empty_errors);
    RUN_TEST(test_problem_roundtrip);

    /* 13.7: Content negotiation */
    RUN_TEST(test_negotiate_exact_match);
    RUN_TEST(test_negotiate_quality_factor);
    RUN_TEST(test_negotiate_wildcard);
    RUN_TEST(test_negotiate_partial_wildcard);
    RUN_TEST(test_negotiate_no_match);

    /* 13.6: Operation lookup */
    RUN_TEST(test_find_operation_exact);
    RUN_TEST(test_find_operation_with_params);
    RUN_TEST(test_find_operation_not_found);
    RUN_TEST(test_find_operation_wrong_method);

    /* 13.2: Header validation */
    RUN_TEST(test_validate_header_pass);
    RUN_TEST(test_validate_header_required_missing);
    RUN_TEST(test_validate_header_optional_missing);
    RUN_TEST(test_validate_header_case_insensitive);

    /* 13.3: Query param validation */
    RUN_TEST(test_validate_query_param_pass);
    RUN_TEST(test_validate_query_required_missing);

    /* 13.4: Path param validation */
    RUN_TEST(test_path_param_integer_pass);
    RUN_TEST(test_path_param_integer_fail);

    return UNITY_END();
}
