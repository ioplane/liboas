#include <liboas/oas_alloc.h>
#include <liboas/oas_error.h>
#include <liboas/oas_parser.h>
#include <liboas/oas_schema.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

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

/* Helper: read file into malloc buffer */
static char *read_file(const char *path, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return nullptr;
    }
    (void)fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    (void)fseek(fp, 0, SEEK_SET);
    if (sz < 0) {
        (void)fclose(fp);
        return nullptr;
    }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        (void)fclose(fp);
        return nullptr;
    }
    size_t nr = fread(buf, 1, (size_t)sz, fp);
    (void)fclose(fp);
    buf[nr] = '\0';
    *out_len = nr;
    return buf;
}

void test_parse_ex_file_ref(void)
{
    size_t len = 0;
    char *json = read_file("tests/fixtures/petstore_with_file_ref.json", &len);
    TEST_ASSERT_NOT_NULL(json);

    oas_ref_options_t opts = {0};
    opts.allow_file = true;
    opts.base_dir = "tests/fixtures";

    oas_doc_t *doc = oas_doc_parse_ex(arena, json, len, &opts, errors);
    TEST_ASSERT_NOT_NULL(doc);

    /* Verify the document parsed correctly */
    TEST_ASSERT_NOT_NULL(doc->info);
    TEST_ASSERT_EQUAL_STRING("Pet Store", doc->info->title);

    /* Verify the file $ref was resolved: /pets GET 200 schema items */
    TEST_ASSERT_EQUAL_size_t(1, doc->paths_count);
    TEST_ASSERT_NOT_NULL(doc->paths[0].item);
    TEST_ASSERT_NOT_NULL(doc->paths[0].item->get);

    oas_operation_t *op = doc->paths[0].item->get;
    TEST_ASSERT_EQUAL_size_t(1, op->responses_count);

    oas_response_t *resp = op->responses[0].response;
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_size_t(1, resp->content_count);

    oas_media_type_t *mt = resp->content[0].value;
    TEST_ASSERT_NOT_NULL(mt);
    TEST_ASSERT_NOT_NULL(mt->schema);

    /* items schema has a $ref that should have been resolved */
    oas_schema_t *items = mt->schema->items;
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_NOT_NULL(items->ref);
    TEST_ASSERT_NOT_NULL(items->ref_resolved);

    /* The resolved schema should be the Pet (object) type */
    TEST_ASSERT_TRUE((items->ref_resolved->type_mask & OAS_TYPE_OBJECT) != 0);

    free(json);
}

void test_parse_ex_file_ref_not_found(void)
{
    /* JSON with a $ref to a nonexistent file */
    const char *json = "{\"openapi\":\"3.1.0\","
                       "\"info\":{\"title\":\"T\",\"version\":\"1.0\"},"
                       "\"paths\":{\"/x\":{\"get\":{"
                       "\"responses\":{\"200\":{\"description\":\"OK\","
                       "\"content\":{\"application/json\":{\"schema\":"
                       "{\"$ref\":\"nonexistent_file.json#/Foo\"}"
                       "}}}}}}}}";

    oas_ref_options_t opts = {0};
    opts.allow_file = true;
    opts.base_dir = "tests/fixtures";

    oas_doc_t *doc = oas_doc_parse_ex(arena, json, strlen(json), &opts, errors);
    /* Doc is still returned even if refs fail */
    TEST_ASSERT_NOT_NULL(doc);

    /* But errors should be accumulated */
    TEST_ASSERT_TRUE(oas_error_list_count(errors) > 0);
}

void test_parse_ex_allow_file_false(void)
{
    const char *json = "{\"openapi\":\"3.1.0\","
                       "\"info\":{\"title\":\"T\",\"version\":\"1.0\"},"
                       "\"paths\":{\"/x\":{\"get\":{"
                       "\"responses\":{\"200\":{\"description\":\"OK\","
                       "\"content\":{\"application/json\":{\"schema\":"
                       "{\"$ref\":\"ref_pet.json#/Pet\"}"
                       "}}}}}}}}";

    oas_ref_options_t opts = {0};
    opts.allow_file = false;

    oas_doc_t *doc = oas_doc_parse_ex(arena, json, strlen(json), &opts, errors);
    TEST_ASSERT_NOT_NULL(doc);

    /* Error about file ref not allowed */
    TEST_ASSERT_TRUE(oas_error_list_count(errors) > 0);
}

void test_parse_ex_http_ref_disabled(void)
{
    const char *json = "{\"openapi\":\"3.1.0\","
                       "\"info\":{\"title\":\"T\",\"version\":\"1.0\"},"
                       "\"paths\":{\"/x\":{\"get\":{"
                       "\"responses\":{\"200\":{\"description\":\"OK\","
                       "\"content\":{\"application/json\":{\"schema\":"
                       "{\"$ref\":\"http://example.com/schema.json\"}"
                       "}}}}}}}}";

    oas_ref_options_t opts = {0};
    opts.allow_file = true;
    opts.allow_remote = false;

    oas_doc_t *doc = oas_doc_parse_ex(arena, json, strlen(json), &opts, errors);
    TEST_ASSERT_NOT_NULL(doc);

    /* Error about remote ref not allowed */
    TEST_ASSERT_TRUE(oas_error_list_count(errors) > 0);
}

/* Callback state for test_parse_ex_user_callback */
static bool callback_invoked;
static int mock_fetch(void *ctx, const char *url, char **out_data, size_t *out_len)
{
    (void)ctx;
    (void)url;
    callback_invoked = true;

    const char *response = "{\"MockSchema\":{\"type\":\"string\"}}";
    size_t rlen = strlen(response);
    *out_data = malloc(rlen + 1);
    if (!*out_data) {
        return -ENOMEM;
    }
    memcpy(*out_data, response, rlen + 1);
    *out_len = rlen;
    return 0;
}

void test_parse_ex_user_callback(void)
{
    const char *json = "{\"openapi\":\"3.1.0\","
                       "\"info\":{\"title\":\"T\",\"version\":\"1.0\"},"
                       "\"paths\":{\"/x\":{\"get\":{"
                       "\"responses\":{\"200\":{\"description\":\"OK\","
                       "\"content\":{\"application/json\":{\"schema\":"
                       "{\"$ref\":\"https://example.com/s.json#/MockSchema\"}"
                       "}}}}}}}}";

    callback_invoked = false;

    oas_ref_options_t opts = {0};
    opts.allow_remote = true;
    opts.allow_file = true;
    opts.fetch = mock_fetch;
    opts.fetch_ctx = nullptr;

    oas_doc_t *doc = oas_doc_parse_ex(arena, json, strlen(json), &opts, errors);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_TRUE(callback_invoked);
}

void test_parse_ex_null_opts_defaults(void)
{
    /* With nullptr opts, oas_doc_parse_ex behaves like oas_doc_parse */
    const char *json = "{\"openapi\":\"3.1.0\","
                       "\"info\":{\"title\":\"Test\",\"version\":\"2.0\"},"
                       "\"paths\":{}}";

    oas_doc_t *doc = oas_doc_parse_ex(arena, json, strlen(json), nullptr, errors);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_NOT_NULL(doc->info);
    TEST_ASSERT_EQUAL_STRING("Test", doc->info->title);
    TEST_ASSERT_EQUAL_STRING("2.0", doc->info->version);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_ex_file_ref);
    RUN_TEST(test_parse_ex_file_ref_not_found);
    RUN_TEST(test_parse_ex_allow_file_false);
    RUN_TEST(test_parse_ex_http_ref_disabled);
    RUN_TEST(test_parse_ex_user_callback);
    RUN_TEST(test_parse_ex_null_opts_defaults);
    return UNITY_END();
}
