#include <liboas/oas_alloc.h>
#include <liboas/oas_error.h>

#include <string.h>
#include <unity.h>

#include "parser/oas_json.h"

static oas_arena_t *arena;
static oas_error_list_t *errors;

void setUp(void)
{
    arena = oas_arena_create(0);
    errors = oas_error_list_create(arena);
}

void tearDown(void)
{
    oas_arena_destroy(arena);
    arena = nullptr;
    errors = nullptr;
}

void test_json_parse_valid(void)
{
    const char *json =
        "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"Test\", \"version\": \"1.0\"}}";
    oas_json_doc_t doc = {0};

    int rc = oas_json_parse(json, strlen(json), &doc, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(doc.doc);
    TEST_ASSERT_NOT_NULL(doc.root);
    TEST_ASSERT_FALSE(oas_error_list_has_errors(errors));

    oas_json_free(&doc);
}

void test_json_parse_invalid(void)
{
    const char *json = "{invalid json}";
    oas_json_doc_t doc = {0};

    int rc = oas_json_parse(json, strlen(json), &doc, errors);
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_NULL(doc.doc);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));
}

void test_json_parse_empty(void)
{
    const char *json = "";
    oas_json_doc_t doc = {0};

    int rc = oas_json_parse(json, 0, &doc, errors);
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_NULL(doc.doc);
}

void test_json_get_str(void)
{
    const char *json = "{\"name\": \"hello\", \"count\": 42}";
    oas_json_doc_t doc = {0};

    int rc = oas_json_parse(json, strlen(json), &doc, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);

    const char *name = oas_json_get_str(doc.root, "name");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("hello", name);

    /* Wrong type — should return nullptr */
    const char *count_str = oas_json_get_str(doc.root, "count");
    TEST_ASSERT_NULL(count_str);

    /* Missing key */
    const char *missing = oas_json_get_str(doc.root, "missing");
    TEST_ASSERT_NULL(missing);

    oas_json_free(&doc);
}

void test_json_get_int_default(void)
{
    const char *json = "{\"count\": 42, \"name\": \"test\"}";
    oas_json_doc_t doc = {0};

    int rc = oas_json_parse(json, strlen(json), &doc, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);

    int64_t count = oas_json_get_int(doc.root, "count", -1);
    TEST_ASSERT_EQUAL_INT64(42, count);

    /* Wrong type — should return default */
    int64_t name_int = oas_json_get_int(doc.root, "name", -1);
    TEST_ASSERT_EQUAL_INT64(-1, name_int);

    /* Missing key — should return default */
    int64_t missing = oas_json_get_int(doc.root, "missing", 99);
    TEST_ASSERT_EQUAL_INT64(99, missing);

    oas_json_free(&doc);
}

void test_json_get_bool_default(void)
{
    const char *json = "{\"flag\": true, \"name\": \"test\"}";
    oas_json_doc_t doc = {0};

    int rc = oas_json_parse(json, strlen(json), &doc, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);

    bool flag = oas_json_get_bool(doc.root, "flag", false);
    TEST_ASSERT_TRUE(flag);

    /* Wrong type — should return default */
    bool name_bool = oas_json_get_bool(doc.root, "name", false);
    TEST_ASSERT_FALSE(name_bool);

    /* Missing key — should return default */
    bool missing = oas_json_get_bool(doc.root, "missing", true);
    TEST_ASSERT_TRUE(missing);

    oas_json_free(&doc);
}

void test_json_get_obj_missing(void)
{
    const char *json = "{\"info\": {\"title\": \"T\"}, \"name\": \"test\"}";
    oas_json_doc_t doc = {0};

    int rc = oas_json_parse(json, strlen(json), &doc, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);

    yyjson_val *info = oas_json_get_obj(doc.root, "info");
    TEST_ASSERT_NOT_NULL(info);

    /* Wrong type — string, not object */
    yyjson_val *name = oas_json_get_obj(doc.root, "name");
    TEST_ASSERT_NULL(name);

    /* Missing key */
    yyjson_val *missing = oas_json_get_obj(doc.root, "missing");
    TEST_ASSERT_NULL(missing);

    oas_json_free(&doc);
}

void test_json_parse_file_not_found(void)
{
    oas_json_doc_t doc = {0};

    int rc = oas_json_parse_file("/nonexistent/path.json", &doc, errors);
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_NULL(doc.doc);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_json_parse_valid);
    RUN_TEST(test_json_parse_invalid);
    RUN_TEST(test_json_parse_empty);
    RUN_TEST(test_json_get_str);
    RUN_TEST(test_json_get_int_default);
    RUN_TEST(test_json_get_bool_default);
    RUN_TEST(test_json_get_obj_missing);
    RUN_TEST(test_json_parse_file_not_found);
    return UNITY_END();
}
