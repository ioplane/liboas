#include <liboas/oas_adapter.h>
#include <liboas/oas_builder.h>
#include <liboas/oas_compiler.h>
#include <liboas/oas_emitter.h>
#include <liboas/oas_parser.h>
#include <liboas/oas_regex.h>
#include <liboas/oas_validator.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include <yyjson.h>

static char *petstore_json;
static size_t petstore_len;

void setUp(void)
{
}
void tearDown(void)
{
}

/* ── Helpers ──────────────────────────────────────────────────────────── */

static char *read_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return nullptr;
    }
    (void)fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        (void)fclose(f);
        return nullptr;
    }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        (void)fclose(f);
        return nullptr;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    (void)fclose(f);
    buf[n] = '\0';
    if (out_len) {
        *out_len = n;
    }
    return buf;
}

/* ── Parse tests ──────────────────────────────────────────────────────── */

void test_petstore_parse(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    oas_doc_t *doc = oas_doc_parse(arena, petstore_json, petstore_len, nullptr);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_STRING("Petstore", doc->info->title);
    TEST_ASSERT_EQUAL_STRING("1.0.0", doc->info->version);
    TEST_ASSERT_EQUAL_size_t(1, doc->servers_count);
    TEST_ASSERT_EQUAL_STRING("https://petstore.example.com/v1", doc->servers[0]->url);
    TEST_ASSERT_EQUAL_size_t(2, doc->paths_count);
    TEST_ASSERT_NOT_NULL(doc->components);
    TEST_ASSERT_EQUAL_size_t(1, doc->components->schemas_count);
    TEST_ASSERT_EQUAL_STRING("Pet", doc->components->schemas[0].name);
    oas_doc_free(doc);
    oas_arena_destroy(arena);
}

void test_petstore_parse_file(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    oas_doc_t *doc = oas_doc_parse_file(arena, "tests/fixtures/petstore.json", nullptr);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_STRING("Petstore", doc->info->title);
    oas_doc_free(doc);
    oas_arena_destroy(arena);
}

void test_petstore_compile(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    oas_doc_t *doc = oas_doc_parse(arena, petstore_json, petstore_len, nullptr);
    TEST_ASSERT_NOT_NULL(doc);

    oas_compiler_config_t config = {
        .regex = oas_regex_libregexp_create(),
        .format_policy = 0,
    };
    oas_compiled_doc_t *compiled = oas_doc_compile(doc, &config, nullptr);
    TEST_ASSERT_NOT_NULL(compiled);

    oas_compiled_doc_free(compiled);
    oas_doc_free(doc);
    oas_arena_destroy(arena);
}

/* ── Request validation tests ─────────────────────────────────────────── */

void test_petstore_validate_get_pets(void)
{
    oas_adapter_t *adapter = oas_adapter_create(petstore_json, petstore_len, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_arena_t *arena = oas_arena_create(0);
    oas_validation_result_t result = {0};

    oas_http_request_t req = {.method = "GET", .path = "/pets"};
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_arena_destroy(arena);
    oas_adapter_destroy(adapter);
}

void test_petstore_validate_create_pet_pass(void)
{
    oas_adapter_t *adapter = oas_adapter_create(petstore_json, petstore_len, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_arena_t *arena = oas_arena_create(0);
    oas_validation_result_t result = {0};

    const char body[] = "{\"id\":1,\"name\":\"Fido\"}";
    oas_http_request_t req = {
        .method = "POST",
        .path = "/pets",
        .content_type = "application/json",
        .body = body,
        .body_len = sizeof(body) - 1,
    };
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    oas_arena_destroy(arena);
    oas_adapter_destroy(adapter);
}

void test_petstore_validate_create_pet_fail(void)
{
    /* Build a spec with inline schema (not $ref) to test validation failure.
     * The petstore fixture uses $ref which requires ref resolution before
     * compilation — tested separately via builder API below. */
    oas_arena_t *build_arena = oas_arena_create(0);
    oas_doc_t *doc = oas_doc_build(build_arena, "Test", "1.0.0");

    oas_schema_t *pet = oas_schema_build_object(build_arena);
    (void)oas_schema_add_property(build_arena, pet, "id", oas_schema_build_int64(build_arena));
    (void)oas_schema_add_property(build_arena, pet, "name", oas_schema_build_string(build_arena));
    (void)oas_schema_set_required(build_arena, pet, "id", "name", nullptr);

    (void)oas_doc_add_path_op(doc, build_arena, "/pets", "post",
                              &(oas_op_builder_t){
                                  .operation_id = "createPet",
                                  .request_body = pet,
                                  .request_body_required = true,
                                  .responses =
                                      (oas_response_builder_t[]){
                                          {.status = 201, .description = "Created"},
                                          {0},
                                      },
                              });

    oas_adapter_t *adapter = oas_adapter_from_doc(doc, build_arena, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_arena_t *arena = oas_arena_create(0);
    oas_validation_result_t result = {0};

    /* Missing required "name" field */
    const char body[] = "{\"id\":1}";
    oas_http_request_t req = {
        .method = "POST",
        .path = "/pets",
        .content_type = "application/json",
        .body = body,
        .body_len = sizeof(body) - 1,
    };
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_arena_destroy(arena);
    oas_adapter_destroy(adapter);
    oas_arena_destroy(build_arena);
}

void test_petstore_validate_response_pass(void)
{
    oas_adapter_t *adapter = oas_adapter_create(petstore_json, petstore_len, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_arena_t *arena = oas_arena_create(0);
    oas_validation_result_t result = {0};

    const char body[] = "[{\"id\":1,\"name\":\"Fido\"},{\"id\":2,\"name\":\"Rex\"}]";
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

void test_petstore_unknown_path(void)
{
    oas_adapter_t *adapter = oas_adapter_create(petstore_json, petstore_len, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_arena_t *arena = oas_arena_create(0);
    oas_validation_result_t result = {0};

    oas_http_request_t req = {.method = "GET", .path = "/unknown"};
    int rc = oas_adapter_validate_request(adapter, &req, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_arena_destroy(arena);
    oas_adapter_destroy(adapter);
}

/* ── Roundtrip tests ──────────────────────────────────────────────────── */

void test_petstore_roundtrip(void)
{
    oas_arena_t *arena = oas_arena_create(0);
    oas_doc_t *doc = oas_doc_parse(arena, petstore_json, petstore_len, nullptr);
    TEST_ASSERT_NOT_NULL(doc);

    size_t len = 0;
    char *json = oas_doc_emit_json(doc, nullptr, &len);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_GREATER_THAN(0, len);

    /* Re-parse emitted JSON */
    yyjson_doc *parsed = yyjson_read(json, len, 0);
    TEST_ASSERT_NOT_NULL(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    yyjson_val *info = yyjson_obj_get(root, "info");
    TEST_ASSERT_EQUAL_STRING("Petstore", yyjson_get_str(yyjson_obj_get(info, "title")));

    yyjson_val *paths = yyjson_obj_get(root, "paths");
    TEST_ASSERT_NOT_NULL(yyjson_obj_get(paths, "/pets"));
    TEST_ASSERT_NOT_NULL(yyjson_obj_get(paths, "/pets/{petId}"));

    yyjson_val *components = yyjson_obj_get(root, "components");
    yyjson_val *schemas = yyjson_obj_get(components, "schemas");
    TEST_ASSERT_NOT_NULL(yyjson_obj_get(schemas, "Pet"));

    yyjson_doc_free(parsed);
    oas_emit_free(json);
    oas_doc_free(doc);
    oas_arena_destroy(arena);
}

/* ── Builder roundtrip tests ──────────────────────────────────────────── */

void test_builder_petstore_emit(void)
{
    oas_arena_t *arena = oas_arena_create(0);

    /* Build petstore from scratch */
    oas_doc_t *doc = oas_doc_build(arena, "Builder Petstore", "1.0.0");
    (void)oas_doc_add_server(doc, arena, "https://petstore.example.com/v1", "Production");

    /* Pet schema */
    oas_schema_t *pet = oas_schema_build_object(arena);
    (void)oas_schema_add_property(arena, pet, "id", oas_schema_build_int64(arena));
    (void)oas_schema_add_property(arena, pet, "name", oas_schema_build_string(arena));
    (void)oas_schema_add_property(arena, pet, "tag", oas_schema_build_string(arena));
    (void)oas_schema_set_required(arena, pet, "id", "name", nullptr);
    (void)oas_doc_add_component_schema(doc, arena, "Pet", pet);

    /* GET /pets */
    (void)oas_doc_add_path_op(
        doc, arena, "/pets", "get",
        &(oas_op_builder_t){
            .summary = "List all pets",
            .operation_id = "listPets",
            .tag = "pets",
            .params =
                (oas_param_builder_t[]){
                    {.name = "limit", .in = "query", .schema = oas_schema_build_int32(arena)},
                    {0},
                },
            .responses =
                (oas_response_builder_t[]){
                    {.status = 200,
                     .description = "A list of pets",
                     .schema = oas_schema_build_array(arena, pet)},
                    {0},
                },
        });

    /* Emit and verify */
    size_t len = 0;
    char *json = oas_doc_emit_json(doc, nullptr, &len);
    TEST_ASSERT_NOT_NULL(json);

    yyjson_doc *parsed = yyjson_read(json, len, 0);
    TEST_ASSERT_NOT_NULL(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    yyjson_val *info = yyjson_obj_get(root, "info");
    TEST_ASSERT_EQUAL_STRING("Builder Petstore", yyjson_get_str(yyjson_obj_get(info, "title")));

    yyjson_val *paths = yyjson_obj_get(root, "paths");
    yyjson_val *pets = yyjson_obj_get(paths, "/pets");
    TEST_ASSERT_NOT_NULL(pets);
    yyjson_val *get = yyjson_obj_get(pets, "get");
    TEST_ASSERT_EQUAL_STRING("listPets", yyjson_get_str(yyjson_obj_get(get, "operationId")));

    yyjson_doc_free(parsed);
    oas_emit_free(json);
    oas_arena_destroy(arena);
}

void test_builder_compile_validate(void)
{
    oas_arena_t *arena = oas_arena_create(0);

    /* Build a minimal API */
    oas_doc_t *doc = oas_doc_build(arena, "Test API", "1.0.0");

    oas_schema_t *item = oas_schema_build_object(arena);
    (void)oas_schema_add_property(arena, item, "name", oas_schema_build_string(arena));
    (void)oas_schema_set_required(arena, item, "name", nullptr);

    (void)oas_doc_add_path_op(doc, arena, "/items", "post",
                              &(oas_op_builder_t){
                                  .operation_id = "createItem",
                                  .request_body = item,
                                  .request_body_required = true,
                                  .responses =
                                      (oas_response_builder_t[]){
                                          {.status = 201, .description = "Created"},
                                          {0},
                                      },
                              });

    /* Create adapter and validate */
    oas_adapter_t *adapter = oas_adapter_from_doc(doc, arena, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(adapter);

    oas_arena_t *val_arena = oas_arena_create(0);
    oas_validation_result_t result = {0};

    /* Valid request */
    const char good[] = "{\"name\":\"widget\"}";
    oas_http_request_t req_good = {
        .method = "POST",
        .path = "/items",
        .content_type = "application/json",
        .body = good,
        .body_len = sizeof(good) - 1,
    };
    int rc = oas_adapter_validate_request(adapter, &req_good, &result, val_arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(result.valid);

    /* Invalid request — missing "name" */
    memset(&result, 0, sizeof(result));
    const char bad[] = "{\"price\":42}";
    oas_http_request_t req_bad = {
        .method = "POST",
        .path = "/items",
        .content_type = "application/json",
        .body = bad,
        .body_len = sizeof(bad) - 1,
    };
    rc = oas_adapter_validate_request(adapter, &req_bad, &result, val_arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    oas_arena_destroy(val_arena);
    oas_adapter_destroy(adapter);
    oas_arena_destroy(arena);
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    /* Load petstore fixture */
    petstore_json = read_file("tests/fixtures/petstore.json", &petstore_len);
    if (!petstore_json) {
        (void)fprintf(stderr, "ERROR: cannot read tests/fixtures/petstore.json\n");
        return 1;
    }

    UNITY_BEGIN();

    /* Parse */
    RUN_TEST(test_petstore_parse);
    RUN_TEST(test_petstore_parse_file);
    RUN_TEST(test_petstore_compile);

    /* Request/response validation */
    RUN_TEST(test_petstore_validate_get_pets);
    RUN_TEST(test_petstore_validate_create_pet_pass);
    RUN_TEST(test_petstore_validate_create_pet_fail);
    RUN_TEST(test_petstore_validate_response_pass);
    RUN_TEST(test_petstore_unknown_path);

    /* Roundtrip */
    RUN_TEST(test_petstore_roundtrip);

    /* Builder */
    RUN_TEST(test_builder_petstore_emit);
    RUN_TEST(test_builder_compile_validate);

    free(petstore_json);
    return UNITY_END();
}
