#include <liboas/oas_builder.h>
#include <liboas/oas_emitter.h>

#include <yyjson.h>

#include <math.h>
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

/* ── Schema builder tests ──────────────────────────────────────────────── */

void test_build_string(void)
{
    oas_schema_t *s = oas_schema_build_string(arena);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, s->type_mask);
    TEST_ASSERT_EQUAL_INT64(-1, s->min_length);
    TEST_ASSERT_EQUAL_INT64(-1, s->max_length);
}

void test_build_int32(void)
{
    oas_schema_t *s = oas_schema_build_int32(arena);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_INTEGER, s->type_mask);
    TEST_ASSERT_EQUAL_STRING("int32", s->format);
}

void test_build_int64(void)
{
    oas_schema_t *s = oas_schema_build_int64(arena);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_INTEGER, s->type_mask);
    TEST_ASSERT_EQUAL_STRING("int64", s->format);
}

void test_build_number(void)
{
    oas_schema_t *s = oas_schema_build_number(arena);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_NUMBER, s->type_mask);
}

void test_build_bool(void)
{
    oas_schema_t *s = oas_schema_build_bool(arena);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_BOOLEAN, s->type_mask);
}

void test_build_string_constrained(void)
{
    oas_schema_t *s = oas_schema_build_string_ex(arena, &(oas_string_opts_t){
                                                            .min_length = 1,
                                                            .max_length = 255,
                                                            .pattern = "^[a-z]+$",
                                                            .format = "email",
                                                            .description = "User email",
                                                        });
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, s->type_mask);
    TEST_ASSERT_EQUAL_INT64(1, s->min_length);
    TEST_ASSERT_EQUAL_INT64(255, s->max_length);
    TEST_ASSERT_EQUAL_STRING("^[a-z]+$", s->pattern);
    TEST_ASSERT_EQUAL_STRING("email", s->format);
    TEST_ASSERT_EQUAL_STRING("User email", s->description);
}

void test_build_integer_constrained(void)
{
    oas_schema_t *s = oas_schema_build_integer_ex(arena, &(oas_number_opts_t){
                                                             .minimum = 0,
                                                             .maximum = 100,
                                                             .multiple_of = (double)NAN,
                                                             .format = "int32",
                                                         });
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_INTEGER, s->type_mask);
    TEST_ASSERT_TRUE(s->has_minimum);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, s->minimum);
    TEST_ASSERT_TRUE(s->has_maximum);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 100.0, s->maximum);
    TEST_ASSERT_FALSE(s->has_multiple_of);
    TEST_ASSERT_EQUAL_STRING("int32", s->format);
}

void test_build_array(void)
{
    oas_schema_t *items = oas_schema_build_string(arena);
    oas_schema_t *arr = oas_schema_build_array(arena, items);
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_ARRAY, arr->type_mask);
    TEST_ASSERT_EQUAL_PTR(items, arr->items);
}

void test_build_object_with_properties(void)
{
    oas_schema_t *obj = oas_schema_build_object(arena);
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_OBJECT, obj->type_mask);

    int rc = oas_schema_add_property(arena, obj, "name", oas_schema_build_string(arena));
    TEST_ASSERT_EQUAL_INT(0, rc);
    rc = oas_schema_add_property(arena, obj, "age", oas_schema_build_int32(arena));
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_EQUAL_size_t(2, obj->properties_count);
}

void test_set_required(void)
{
    oas_schema_t *obj = oas_schema_build_object(arena);
    (void)oas_schema_add_property(arena, obj, "id", oas_schema_build_int64(arena));
    (void)oas_schema_add_property(arena, obj, "name", oas_schema_build_string(arena));

    int rc = oas_schema_set_required(arena, obj, "id", "name", nullptr);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_size_t(2, obj->required_count);
    TEST_ASSERT_EQUAL_STRING("id", obj->required[0]);
    TEST_ASSERT_EQUAL_STRING("name", obj->required[1]);
}

/* ── Document builder tests ────────────────────────────────────────────── */

void test_build_doc_minimal(void)
{
    oas_doc_t *doc = oas_doc_build(arena, "Test API", "1.0.0");
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_STRING("3.2.0", doc->openapi);
    TEST_ASSERT_NOT_NULL(doc->info);
    TEST_ASSERT_EQUAL_STRING("Test API", doc->info->title);
    TEST_ASSERT_EQUAL_STRING("1.0.0", doc->info->version);
}

void test_add_server(void)
{
    oas_doc_t *doc = oas_doc_build(arena, "Test", "1.0.0");
    int rc = oas_doc_add_server(doc, arena, "https://api.example.com", "Production");
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_size_t(1, doc->servers_count);
    TEST_ASSERT_EQUAL_STRING("https://api.example.com", doc->servers[0]->url);
    TEST_ASSERT_EQUAL_STRING("Production", doc->servers[0]->description);
}

void test_add_component_schema(void)
{
    oas_doc_t *doc = oas_doc_build(arena, "Test", "1.0.0");
    oas_schema_t *pet = oas_schema_build_object(arena);
    (void)oas_schema_add_property(arena, pet, "name", oas_schema_build_string(arena));

    int rc = oas_doc_add_component_schema(doc, arena, "Pet", pet);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(doc->components);
    TEST_ASSERT_EQUAL_size_t(1, doc->components->schemas_count);
    TEST_ASSERT_EQUAL_STRING("Pet", doc->components->schemas[0].name);
    TEST_ASSERT_EQUAL_PTR(pet, doc->components->schemas[0].schema);
}

void test_add_path_op_with_params(void)
{
    oas_doc_t *doc = oas_doc_build(arena, "Test", "1.0.0");

    int rc = oas_doc_add_path_op(
        doc, arena, "/pets", "get",
        &(oas_op_builder_t){
            .summary = "List pets",
            .operation_id = "listPets",
            .params =
                (oas_param_builder_t[]){
                    {.name = "limit", .in = "query", .schema = oas_schema_build_int32(arena)},
                    {0},
                },
            .responses =
                (oas_response_builder_t[]){
                    {.status = 200,
                     .description = "OK",
                     .schema = oas_schema_build_array(arena, oas_schema_build_string(arena))},
                    {0},
                },
        });
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_size_t(1, doc->paths_count);
    TEST_ASSERT_NOT_NULL(doc->paths[0].item->get);
    TEST_ASSERT_EQUAL_STRING("listPets", doc->paths[0].item->get->operation_id);
    TEST_ASSERT_EQUAL_size_t(1, doc->paths[0].item->get->parameters_count);
    TEST_ASSERT_EQUAL_STRING("limit", doc->paths[0].item->get->parameters[0]->name);
}

void test_add_path_op_with_request_body(void)
{
    oas_doc_t *doc = oas_doc_build(arena, "Test", "1.0.0");

    oas_schema_t *body = oas_schema_build_object(arena);
    (void)oas_schema_add_property(arena, body, "name", oas_schema_build_string(arena));

    int rc = oas_doc_add_path_op(doc, arena, "/pets", "post",
                                 &(oas_op_builder_t){
                                     .summary = "Create pet",
                                     .operation_id = "createPet",
                                     .request_body = body,
                                     .request_body_required = true,
                                     .responses =
                                         (oas_response_builder_t[]){
                                             {.status = 201, .description = "Created"},
                                             {0},
                                         },
                                 });
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(doc->paths[0].item->post);
    TEST_ASSERT_NOT_NULL(doc->paths[0].item->post->request_body);
    TEST_ASSERT_TRUE(doc->paths[0].item->post->request_body->required);
}

void test_roundtrip_emit_reparse(void)
{
    oas_doc_t *doc = oas_doc_build(arena, "Roundtrip API", "2.0.0");
    (void)oas_doc_add_server(doc, arena, "https://api.example.com", nullptr);

    oas_schema_t *pet = oas_schema_build_object(arena);
    (void)oas_schema_add_property(arena, pet, "id", oas_schema_build_int64(arena));
    (void)oas_schema_add_property(arena, pet, "name", oas_schema_build_string(arena));
    (void)oas_schema_set_required(arena, pet, "id", "name", nullptr);
    (void)oas_doc_add_component_schema(doc, arena, "Pet", pet);

    (void)oas_doc_add_path_op(doc, arena, "/pets", "get",
                              &(oas_op_builder_t){
                                  .summary = "List pets",
                                  .operation_id = "listPets",
                                  .tag = "pets",
                                  .responses =
                                      (oas_response_builder_t[]){
                                          {.status = 200,
                                           .description = "OK",
                                           .schema = oas_schema_build_array(arena, pet)},
                                          {0},
                                      },
                              });

    /* Emit to JSON */
    size_t len = 0;
    char *json = oas_doc_emit_json(doc, nullptr, &len);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_GREATER_THAN(0, len);

    /* Re-parse and verify key fields */
    yyjson_doc *parsed = yyjson_read(json, len, 0);
    TEST_ASSERT_NOT_NULL(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    TEST_ASSERT_EQUAL_STRING("3.2.0", yyjson_get_str(yyjson_obj_get(root, "openapi")));

    yyjson_val *info = yyjson_obj_get(root, "info");
    TEST_ASSERT_EQUAL_STRING("Roundtrip API", yyjson_get_str(yyjson_obj_get(info, "title")));

    yyjson_val *servers = yyjson_obj_get(root, "servers");
    TEST_ASSERT_NOT_NULL(servers);
    TEST_ASSERT_EQUAL_size_t(1, yyjson_arr_size(servers));

    yyjson_val *paths = yyjson_obj_get(root, "paths");
    TEST_ASSERT_NOT_NULL(paths);
    yyjson_val *pets_path = yyjson_obj_get(paths, "/pets");
    TEST_ASSERT_NOT_NULL(pets_path);
    yyjson_val *get = yyjson_obj_get(pets_path, "get");
    TEST_ASSERT_EQUAL_STRING("listPets", yyjson_get_str(yyjson_obj_get(get, "operationId")));

    yyjson_val *components = yyjson_obj_get(root, "components");
    TEST_ASSERT_NOT_NULL(components);
    yyjson_val *schemas = yyjson_obj_get(components, "schemas");
    yyjson_val *pet_schema = yyjson_obj_get(schemas, "Pet");
    TEST_ASSERT_NOT_NULL(pet_schema);

    yyjson_doc_free(parsed);
    oas_emit_free(json);
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_string);
    RUN_TEST(test_build_int32);
    RUN_TEST(test_build_int64);
    RUN_TEST(test_build_number);
    RUN_TEST(test_build_bool);
    RUN_TEST(test_build_string_constrained);
    RUN_TEST(test_build_integer_constrained);
    RUN_TEST(test_build_array);
    RUN_TEST(test_build_object_with_properties);
    RUN_TEST(test_set_required);
    RUN_TEST(test_build_doc_minimal);
    RUN_TEST(test_add_server);
    RUN_TEST(test_add_component_schema);
    RUN_TEST(test_add_path_op_with_params);
    RUN_TEST(test_add_path_op_with_request_body);
    RUN_TEST(test_roundtrip_emit_reparse);
    return UNITY_END();
}
