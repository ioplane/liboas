#include <liboas/oas_adapter.h>
#include <liboas/oas_builder.h>

#include <stdlib.h>
#include <string.h>
#include <unity.h>

void setUp(void)
{
}
void tearDown(void)
{
}

/* Minimal valid OpenAPI spec */
static const char MINIMAL_SPEC[] =
    "{"
    "\"openapi\":\"3.1.0\","
    "\"info\":{\"title\":\"Test\",\"version\":\"1.0.0\"},"
    "\"paths\":{"
    "\"/pets\":{\"get\":{\"operationId\":\"listPets\","
    "\"responses\":{\"200\":{\"description\":\"OK\","
    "\"content\":{\"application/json\":{\"schema\":{\"type\":\"array\","
    "\"items\":{\"type\":\"object\","
    "\"properties\":{\"name\":{\"type\":\"string\"}},"
    "\"required\":[\"name\"]"
    "}}}}}}}}}"
    "}";

/* ── Adapter tests ────────────────────────────────────────────────────── */

void test_adapter_create_from_json(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(MINIMAL_SPEC, sizeof(MINIMAL_SPEC) - 1, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    const oas_doc_t *doc = oas_adapter_doc(adapter);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_STRING("Test", doc->info->title);

    oas_adapter_destroy(adapter);
}

void test_adapter_create_invalid_spec(void)
{
    const char bad[] = "{\"not\":\"openapi\"}";
    oas_adapter_t *adapter = oas_adapter_create(bad, sizeof(bad) - 1, nullptr, nullptr);
    TEST_ASSERT_NULL(adapter);
}

void test_adapter_create_null_safe(void)
{
    TEST_ASSERT_NULL(oas_adapter_create(nullptr, 0, nullptr, nullptr));
    TEST_ASSERT_NULL(oas_adapter_from_doc(nullptr, nullptr, nullptr, nullptr));
    oas_adapter_destroy(nullptr);
    TEST_ASSERT_NULL(oas_adapter_doc(nullptr));
    TEST_ASSERT_NULL(oas_adapter_config(nullptr));
    TEST_ASSERT_NULL(oas_adapter_spec_json(nullptr, nullptr));
}

void test_adapter_from_doc(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    oas_doc_t *doc = oas_doc_build(arena, "Code-First API", "2.0.0");

    oas_schema_t *pet = oas_schema_build_object(arena);
    (void)oas_schema_add_property(arena, pet, "name", oas_schema_build_string(arena));
    (void)oas_schema_set_required(arena, pet, "name", nullptr);

    (void)oas_doc_add_path_op(doc, arena, "/pets", "get",
                              &(oas_op_builder_t){
                                  .summary = "List pets",
                                  .operation_id = "listPets",
                                  .responses =
                                      (oas_response_builder_t[]){
                                          {.status = 200,
                                           .description = "OK",
                                           .schema = oas_schema_build_array(arena, pet)},
                                          {0},
                                      },
                              });

    oas_adapter_t *adapter = oas_adapter_from_doc(doc, arena, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    const oas_doc_t *got = oas_adapter_doc(adapter);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_STRING("Code-First API", got->info->title);

    oas_adapter_destroy(adapter);
    oas_arena_destroy(arena);
}

void test_adapter_config_defaults(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(MINIMAL_SPEC, sizeof(MINIMAL_SPEC) - 1, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    const oas_adapter_config_t *cfg = oas_adapter_config(adapter);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_TRUE(cfg->validate_requests);
    TEST_ASSERT_FALSE(cfg->validate_responses);
    TEST_ASSERT_FALSE(cfg->serve_spec);
    TEST_ASSERT_FALSE(cfg->serve_scalar);
    TEST_ASSERT_EQUAL_STRING("/openapi.json", cfg->spec_url);
    TEST_ASSERT_EQUAL_STRING("/docs", cfg->docs_url);

    oas_adapter_destroy(adapter);
}

void test_adapter_spec_json(void)
{
    oas_adapter_config_t config = {
        .serve_spec = true,
    };
    oas_adapter_t *adapter =
        oas_adapter_create(MINIMAL_SPEC, sizeof(MINIMAL_SPEC) - 1, &config, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    size_t len = 0;
    const char *json = oas_adapter_spec_json(adapter, &len);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_GREATER_THAN(0, len);
    /* Should contain openapi version */
    TEST_ASSERT_NOT_NULL(strstr(json, "\"openapi\""));

    oas_adapter_destroy(adapter);
}

void test_adapter_validate_request(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(MINIMAL_SPEC, sizeof(MINIMAL_SPEC) - 1, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_arena_t *arena = oas_arena_create(0);
    oas_validation_result_t result = {0};

    oas_http_request_t req = {
        .method = "GET",
        .path = "/pets",
    };
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_arena_destroy(arena);
    oas_adapter_destroy(adapter);
}

void test_adapter_validate_response(void)
{
    oas_adapter_t *adapter =
        oas_adapter_create(MINIMAL_SPEC, sizeof(MINIMAL_SPEC) - 1, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_arena_t *arena = oas_arena_create(0);
    oas_validation_result_t result = {0};

    const char body[] = "[{\"name\":\"Fido\"}]";
    oas_http_response_t resp = {
        .status_code = 200,
        .content_type = "application/json",
        .body = body,
        .body_len = sizeof(body) - 1,
    };
    int rc = oas_adapter_validate_response(adapter, "/pets", "GET", &resp, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_arena_destroy(arena);
    oas_adapter_destroy(adapter);
}

/* ── Scalar UI tests ──────────────────────────────────────────────────── */

void test_scalar_contains_spec_url(void)
{
    size_t len = 0;
    char *html = oas_scalar_html("My API", "/openapi.json", &len);
    TEST_ASSERT_NOT_NULL(html);
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(html, "data-url=\"/openapi.json\""));
    free(html);
}

void test_scalar_contains_title(void)
{
    char *html = oas_scalar_html("Pet Store API", "/spec.json", nullptr);
    TEST_ASSERT_NOT_NULL(html);
    TEST_ASSERT_NOT_NULL(strstr(html, "<title>Pet Store API</title>"));
    free(html);
}

void test_scalar_default_title(void)
{
    char *html = oas_scalar_html(nullptr, "/api.json", nullptr);
    TEST_ASSERT_NOT_NULL(html);
    TEST_ASSERT_NOT_NULL(strstr(html, "<title>API Documentation</title>"));
    free(html);
}

void test_scalar_null_safe(void)
{
    TEST_ASSERT_NULL(oas_scalar_html(nullptr, nullptr, nullptr));
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* Adapter */
    RUN_TEST(test_adapter_create_from_json);
    RUN_TEST(test_adapter_create_invalid_spec);
    RUN_TEST(test_adapter_create_null_safe);
    RUN_TEST(test_adapter_from_doc);
    RUN_TEST(test_adapter_config_defaults);
    RUN_TEST(test_adapter_spec_json);
    RUN_TEST(test_adapter_validate_request);
    RUN_TEST(test_adapter_validate_response);

    /* Scalar UI */
    RUN_TEST(test_scalar_contains_spec_url);
    RUN_TEST(test_scalar_contains_title);
    RUN_TEST(test_scalar_default_title);
    RUN_TEST(test_scalar_null_safe);

    return UNITY_END();
}
