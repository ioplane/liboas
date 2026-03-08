#include <liboas/oas_parser.h>

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

static oas_doc_t *parse_str(const char *json)
{
    return oas_doc_parse(arena, json, strlen(json), errors);
}

void test_doc_parse_minimal(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_STRING("3.2.0", doc->openapi);
    TEST_ASSERT_NOT_NULL(doc->info);
    TEST_ASSERT_EQUAL_STRING("T", doc->info->title);
    TEST_ASSERT_EQUAL_STRING("1", doc->info->version);
}

void test_doc_parse_petstore(void)
{
    oas_doc_t *doc = oas_doc_parse_file(arena, "tests/fixtures/petstore.json", errors);
    TEST_ASSERT_NOT_NULL_MESSAGE(doc, "petstore.json should parse successfully");
    TEST_ASSERT_EQUAL_STRING("3.2.0", doc->openapi);
    TEST_ASSERT_EQUAL_STRING("Petstore", doc->info->title);
    TEST_ASSERT_TRUE(doc->paths_count >= 2);
    TEST_ASSERT_NOT_NULL(doc->components);
    TEST_ASSERT_TRUE(doc->components->schemas_count >= 1);
}

void test_doc_parse_invalid_json(void)
{
    oas_doc_t *doc = parse_str("{not valid json}");
    TEST_ASSERT_NULL(doc);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));
}

void test_doc_parse_missing_openapi(void)
{
    oas_doc_t *doc = parse_str("{\"info\": {\"title\": \"T\", \"version\": \"1\"}}");
    TEST_ASSERT_NULL(doc);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));
}

void test_doc_parse_wrong_version(void)
{
    oas_doc_t *doc =
        parse_str("{\"openapi\": \"2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}}");
    TEST_ASSERT_NULL(doc);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));
}

void test_doc_parse_info_fields(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"My API\", "
                       "\"description\": \"A test\", \"version\": \"2.0.0\", "
                       "\"contact\": {\"name\": \"Dev\", \"email\": \"dev@test.com\"}, "
                       "\"license\": {\"name\": \"MIT\"}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_STRING("My API", doc->info->title);
    TEST_ASSERT_EQUAL_STRING("A test", doc->info->description);
    TEST_ASSERT_NOT_NULL(doc->info->contact);
    TEST_ASSERT_EQUAL_STRING("Dev", doc->info->contact->name);
    TEST_ASSERT_NOT_NULL(doc->info->license);
    TEST_ASSERT_EQUAL_STRING("MIT", doc->info->license->name);
}

void test_doc_parse_servers(void)
{
    const char *json =
        "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
        "\"servers\": [{\"url\": \"https://api.test.com\", \"description\": \"Prod\"}]}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_UINT64(1, doc->servers_count);
    TEST_ASSERT_EQUAL_STRING("https://api.test.com", doc->servers[0]->url);
    TEST_ASSERT_EQUAL_STRING("Prod", doc->servers[0]->description);
}

void test_doc_parse_paths(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"paths\": {\"/pets\": {\"get\": {\"operationId\": \"listPets\", "
                       "\"responses\": {\"200\": {\"description\": \"OK\"}}}}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_UINT64(1, doc->paths_count);
    TEST_ASSERT_EQUAL_STRING("/pets", doc->paths[0].path);
    TEST_ASSERT_NOT_NULL(doc->paths[0].item);
    TEST_ASSERT_NOT_NULL(doc->paths[0].item->get);
    TEST_ASSERT_EQUAL_STRING("listPets", doc->paths[0].item->get->operation_id);
}

void test_doc_parse_operations(void)
{
    const char *json =
        "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
        "\"paths\": {\"/pets\": {"
        "\"get\": {\"operationId\": \"list\", \"summary\": \"List\", "
        "\"tags\": [\"pets\"], \"responses\": {\"200\": {\"description\": \"OK\"}}}, "
        "\"post\": {\"operationId\": \"create\", \"deprecated\": true, "
        "\"responses\": {\"201\": {\"description\": \"Created\"}}}"
        "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);

    oas_path_item_t *pi = doc->paths[0].item;
    TEST_ASSERT_NOT_NULL(pi->get);
    TEST_ASSERT_NOT_NULL(pi->post);
    TEST_ASSERT_NULL(pi->put);

    TEST_ASSERT_EQUAL_STRING("list", pi->get->operation_id);
    TEST_ASSERT_EQUAL_UINT64(1, pi->get->tags_count);
    TEST_ASSERT_EQUAL_STRING("pets", pi->get->tags[0]);

    TEST_ASSERT_TRUE(pi->post->deprecated);
}

void test_doc_parse_components_schemas(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"components\": {\"schemas\": {"
                       "\"Pet\": {\"type\": \"object\", \"properties\": {"
                       "\"name\": {\"type\": \"string\"}}}"
                       "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_NOT_NULL(doc->components);
    TEST_ASSERT_EQUAL_UINT64(1, doc->components->schemas_count);
    TEST_ASSERT_EQUAL_STRING("Pet", doc->components->schemas[0].name);
    TEST_ASSERT_NOT_NULL(doc->components->schemas[0].schema);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_OBJECT, doc->components->schemas[0].schema->type_mask);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_doc_parse_minimal);
    RUN_TEST(test_doc_parse_petstore);
    RUN_TEST(test_doc_parse_invalid_json);
    RUN_TEST(test_doc_parse_missing_openapi);
    RUN_TEST(test_doc_parse_wrong_version);
    RUN_TEST(test_doc_parse_info_fields);
    RUN_TEST(test_doc_parse_servers);
    RUN_TEST(test_doc_parse_paths);
    RUN_TEST(test_doc_parse_operations);
    RUN_TEST(test_doc_parse_components_schemas);
    return UNITY_END();
}
