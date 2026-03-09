#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include <yyjson.h>

#include "parser/oas_yaml.h"

static constexpr size_t ERR_BUF_SIZE = 256;
static char err_buf[256];

void setUp(void)
{
    memset(err_buf, 0, sizeof(err_buf));
}

void tearDown(void)
{
}

void test_yaml_parse_simple(void)
{
    const char *yaml = "key: value\n";
    yyjson_doc *doc = oas_yaml_to_json(yaml, strlen(yaml), err_buf, ERR_BUF_SIZE);
    TEST_ASSERT_NOT_NULL(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(yyjson_is_obj(root));

    yyjson_val *val = yyjson_obj_get(root, "key");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_TRUE(yyjson_is_str(val));
    TEST_ASSERT_EQUAL_STRING("value", yyjson_get_str(val));

    yyjson_doc_free(doc);
}

void test_yaml_parse_nested(void)
{
    const char *yaml = "info:\n"
                       "  title: Test API\n"
                       "  version: \"1.0\"\n"
                       "  contact:\n"
                       "    name: Admin\n";
    yyjson_doc *doc = oas_yaml_to_json(yaml, strlen(yaml), err_buf, ERR_BUF_SIZE);
    TEST_ASSERT_NOT_NULL(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *info = yyjson_obj_get(root, "info");
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_TRUE(yyjson_is_obj(info));

    yyjson_val *title = yyjson_obj_get(info, "title");
    TEST_ASSERT_EQUAL_STRING("Test API", yyjson_get_str(title));

    /* Quoted string "1.0" must stay a string, not become a number */
    yyjson_val *version = yyjson_obj_get(info, "version");
    TEST_ASSERT_NOT_NULL(version);
    TEST_ASSERT_TRUE(yyjson_is_str(version));
    TEST_ASSERT_EQUAL_STRING("1.0", yyjson_get_str(version));

    yyjson_val *contact = yyjson_obj_get(info, "contact");
    TEST_ASSERT_NOT_NULL(contact);
    yyjson_val *name = yyjson_obj_get(contact, "name");
    TEST_ASSERT_EQUAL_STRING("Admin", yyjson_get_str(name));

    yyjson_doc_free(doc);
}

void test_yaml_parse_types(void)
{
    const char *yaml = "count: 42\n"
                       "ratio: 3.14\n"
                       "enabled: true\n"
                       "disabled: false\n"
                       "nothing: null\n"
                       "tilde: ~\n"
                       "quoted_num: \"200\"\n"
                       "quoted_bool: \"true\"\n";
    yyjson_doc *doc = oas_yaml_to_json(yaml, strlen(yaml), err_buf, ERR_BUF_SIZE);
    TEST_ASSERT_NOT_NULL(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    /* Plain integer */
    yyjson_val *count = yyjson_obj_get(root, "count");
    TEST_ASSERT_TRUE(yyjson_is_int(count));
    TEST_ASSERT_EQUAL_INT64(42, yyjson_get_sint(count));

    /* Plain float */
    yyjson_val *ratio = yyjson_obj_get(root, "ratio");
    TEST_ASSERT_TRUE(yyjson_is_real(ratio));
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 3.14, yyjson_get_real(ratio));

    /* Plain booleans */
    yyjson_val *enabled = yyjson_obj_get(root, "enabled");
    TEST_ASSERT_TRUE(yyjson_is_bool(enabled));
    TEST_ASSERT_TRUE(yyjson_get_bool(enabled));

    yyjson_val *disabled = yyjson_obj_get(root, "disabled");
    TEST_ASSERT_TRUE(yyjson_is_bool(disabled));
    TEST_ASSERT_FALSE(yyjson_get_bool(disabled));

    /* Plain null variants */
    yyjson_val *nothing = yyjson_obj_get(root, "nothing");
    TEST_ASSERT_TRUE(yyjson_is_null(nothing));

    yyjson_val *tilde = yyjson_obj_get(root, "tilde");
    TEST_ASSERT_TRUE(yyjson_is_null(tilde));

    /* Quoted values stay as strings */
    yyjson_val *qnum = yyjson_obj_get(root, "quoted_num");
    TEST_ASSERT_TRUE(yyjson_is_str(qnum));
    TEST_ASSERT_EQUAL_STRING("200", yyjson_get_str(qnum));

    yyjson_val *qbool = yyjson_obj_get(root, "quoted_bool");
    TEST_ASSERT_TRUE(yyjson_is_str(qbool));
    TEST_ASSERT_EQUAL_STRING("true", yyjson_get_str(qbool));

    yyjson_doc_free(doc);
}

void test_yaml_parse_sequence(void)
{
    const char *yaml = "tags:\n"
                       "  - name: users\n"
                       "    description: User operations\n"
                       "  - name: admin\n"
                       "    description: Admin operations\n";
    yyjson_doc *doc = oas_yaml_to_json(yaml, strlen(yaml), err_buf, ERR_BUF_SIZE);
    TEST_ASSERT_NOT_NULL(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tags = yyjson_obj_get(root, "tags");
    TEST_ASSERT_NOT_NULL(tags);
    TEST_ASSERT_TRUE(yyjson_is_arr(tags));
    TEST_ASSERT_EQUAL_size_t(2, yyjson_arr_size(tags));

    yyjson_val *first = yyjson_arr_get(tags, 0);
    TEST_ASSERT_TRUE(yyjson_is_obj(first));
    yyjson_val *name = yyjson_obj_get(first, "name");
    TEST_ASSERT_EQUAL_STRING("users", yyjson_get_str(name));

    yyjson_doc_free(doc);
}

void test_yaml_parse_anchors(void)
{
    /* YAML anchors and aliases — scalar alias */
    const char *yaml = "name: &myname John\n"
                       "greeting: *myname\n";
    yyjson_doc *doc = oas_yaml_to_json(yaml, strlen(yaml), err_buf, ERR_BUF_SIZE);
    TEST_ASSERT_NOT_NULL(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    /* name should be "John" */
    yyjson_val *name = yyjson_obj_get(root, "name");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("John", yyjson_get_str(name));

    /* greeting should also be "John" (via alias) */
    yyjson_val *greeting = yyjson_obj_get(root, "greeting");
    TEST_ASSERT_NOT_NULL(greeting);
    TEST_ASSERT_EQUAL_STRING("John", yyjson_get_str(greeting));

    yyjson_doc_free(doc);
}

void test_yaml_parse_invalid(void)
{
    const char *yaml = ":\n  :\n   - }{";
    yyjson_doc *doc = oas_yaml_to_json(yaml, strlen(yaml), err_buf, ERR_BUF_SIZE);
    TEST_ASSERT_NULL(doc);
    TEST_ASSERT_GREATER_THAN(0, strlen(err_buf));
}

void test_yaml_file_parse(void)
{
    const char *path = "/tmp/test_liboas_yaml.yaml";
    const char *yaml = "openapi: \"3.2.0\"\n"
                       "info:\n"
                       "  title: File Test\n"
                       "  version: \"0.1.0\"\n";

    /* Write temp file */
    FILE *f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs(yaml, f);
    fclose(f);

    yyjson_doc *doc = oas_yaml_file_to_json(path, err_buf, ERR_BUF_SIZE);
    TEST_ASSERT_NOT_NULL(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *openapi = yyjson_obj_get(root, "openapi");
    TEST_ASSERT_EQUAL_STRING("3.2.0", yyjson_get_str(openapi));

    yyjson_val *info = yyjson_obj_get(root, "info");
    yyjson_val *title = yyjson_obj_get(info, "title");
    TEST_ASSERT_EQUAL_STRING("File Test", yyjson_get_str(title));

    yyjson_doc_free(doc);
    remove(path);
}

void test_yaml_auto_detect_json(void)
{
    const char *json = "{\"openapi\": \"3.2.0\"}";
    yyjson_doc *doc = oas_auto_parse(json, strlen(json), err_buf, ERR_BUF_SIZE);
    TEST_ASSERT_NOT_NULL(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *val = yyjson_obj_get(root, "openapi");
    TEST_ASSERT_EQUAL_STRING("3.2.0", yyjson_get_str(val));

    yyjson_doc_free(doc);
}

void test_yaml_auto_detect_yaml(void)
{
    const char *yaml = "openapi: \"3.2.0\"\ninfo:\n  title: Auto\n";
    yyjson_doc *doc = oas_auto_parse(yaml, strlen(yaml), err_buf, ERR_BUF_SIZE);
    TEST_ASSERT_NOT_NULL(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *val = yyjson_obj_get(root, "openapi");
    TEST_ASSERT_EQUAL_STRING("3.2.0", yyjson_get_str(val));

    yyjson_val *info = yyjson_obj_get(root, "info");
    yyjson_val *title = yyjson_obj_get(info, "title");
    TEST_ASSERT_EQUAL_STRING("Auto", yyjson_get_str(title));

    yyjson_doc_free(doc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_yaml_parse_simple);
    RUN_TEST(test_yaml_parse_nested);
    RUN_TEST(test_yaml_parse_types);
    RUN_TEST(test_yaml_parse_sequence);
    RUN_TEST(test_yaml_parse_anchors);
    RUN_TEST(test_yaml_parse_invalid);
    RUN_TEST(test_yaml_file_parse);
    RUN_TEST(test_yaml_auto_detect_json);
    RUN_TEST(test_yaml_auto_detect_yaml);
    return UNITY_END();
}
