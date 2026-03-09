#include <liboas/oas_emitter.h>

#include <liboas/oas_alloc.h>
#include <liboas/oas_doc.h>
#include <liboas/oas_schema.h>

#include <yyjson.h>
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

/* ── Helpers ────────────────────────────────────────────────────────────── */

static oas_doc_t *make_doc(const char *title, const char *version)
{
    oas_doc_t *doc = oas_arena_alloc(arena, sizeof(*doc), _Alignof(oas_doc_t));
    memset(doc, 0, sizeof(*doc));
    doc->openapi = "3.2.0";

    oas_info_t *info = oas_arena_alloc(arena, sizeof(*info), _Alignof(oas_info_t));
    memset(info, 0, sizeof(*info));
    info->title = title;
    info->version = version;
    doc->info = info;

    return doc;
}

static oas_schema_t *make_schema(uint8_t type_mask)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = type_mask;
    return s;
}

/* ── Tests ──────────────────────────────────────────────────────────────── */

void test_emitter_minimal_doc(void)
{
    oas_doc_t *doc = make_doc("Minimal", "1.0.0");

    size_t len = 0;
    char *json = oas_doc_emit_json(doc, nullptr, &len);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_GREATER_THAN(0, len);

    yyjson_doc *parsed = yyjson_read(json, len, 0);
    TEST_ASSERT_NOT_NULL(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    TEST_ASSERT_NOT_NULL(root);

    yyjson_val *openapi = yyjson_obj_get(root, "openapi");
    TEST_ASSERT_NOT_NULL(openapi);
    TEST_ASSERT_EQUAL_STRING("3.2.0", yyjson_get_str(openapi));

    yyjson_val *info = yyjson_obj_get(root, "info");
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_STRING("Minimal", yyjson_get_str(yyjson_obj_get(info, "title")));
    TEST_ASSERT_EQUAL_STRING("1.0.0", yyjson_get_str(yyjson_obj_get(info, "version")));

    yyjson_doc_free(parsed);
    oas_emit_free(json);
}

void test_emitter_with_paths(void)
{
    oas_doc_t *doc = make_doc("Petstore", "1.0.0");

    /* Build a GET /pets operation */
    oas_operation_t *get_op = oas_arena_alloc(arena, sizeof(*get_op), _Alignof(oas_operation_t));
    memset(get_op, 0, sizeof(*get_op));
    get_op->operation_id = "listPets";
    get_op->summary = "List all pets";

    oas_path_item_t *item = oas_arena_alloc(arena, sizeof(*item), _Alignof(oas_path_item_t));
    memset(item, 0, sizeof(*item));
    item->path = "/pets";
    item->get = get_op;

    oas_path_entry_t entry = {.path = "/pets", .item = item};
    doc->paths = &entry;
    doc->paths_count = 1;

    char *json = oas_doc_emit_json(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(json);

    yyjson_doc *parsed = yyjson_read(json, strlen(json), 0);
    TEST_ASSERT_NOT_NULL(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    yyjson_val *paths = yyjson_obj_get(root, "paths");
    TEST_ASSERT_NOT_NULL(paths);

    yyjson_val *pets = yyjson_obj_get(paths, "/pets");
    TEST_ASSERT_NOT_NULL(pets);

    yyjson_val *get = yyjson_obj_get(pets, "get");
    TEST_ASSERT_NOT_NULL(get);
    TEST_ASSERT_EQUAL_STRING("listPets", yyjson_get_str(yyjson_obj_get(get, "operationId")));

    yyjson_doc_free(parsed);
    oas_emit_free(json);
}

void test_emitter_with_components(void)
{
    oas_doc_t *doc = make_doc("Components", "1.0.0");

    oas_schema_t *pet_schema = make_schema(OAS_TYPE_OBJECT);
    pet_schema->title = "Pet";

    oas_schema_entry_t schema_entry = {.name = "Pet", .schema = pet_schema};

    oas_components_t *comp = oas_arena_alloc(arena, sizeof(*comp), _Alignof(oas_components_t));
    memset(comp, 0, sizeof(*comp));
    comp->schemas = &schema_entry;
    comp->schemas_count = 1;
    doc->components = comp;

    char *json = oas_doc_emit_json(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(json);

    yyjson_doc *parsed = yyjson_read(json, strlen(json), 0);
    TEST_ASSERT_NOT_NULL(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    yyjson_val *components = yyjson_obj_get(root, "components");
    TEST_ASSERT_NOT_NULL(components);

    yyjson_val *schemas = yyjson_obj_get(components, "schemas");
    TEST_ASSERT_NOT_NULL(schemas);

    yyjson_val *pet = yyjson_obj_get(schemas, "Pet");
    TEST_ASSERT_NOT_NULL(pet);
    TEST_ASSERT_EQUAL_STRING("Pet", yyjson_get_str(yyjson_obj_get(pet, "title")));

    yyjson_doc_free(parsed);
    oas_emit_free(json);
}

void test_emitter_pretty(void)
{
    oas_doc_t *doc = make_doc("Pretty", "1.0.0");

    oas_emit_options_t opts = {.pretty = true};
    char *json = oas_doc_emit_json(doc, &opts, nullptr);
    TEST_ASSERT_NOT_NULL(json);

    /* Pretty output contains newlines */
    TEST_ASSERT_NOT_NULL(strchr(json, '\n'));

    oas_emit_free(json);
}

void test_emitter_compact(void)
{
    oas_doc_t *doc = make_doc("Compact", "1.0.0");

    oas_emit_options_t opts = {.pretty = false};
    char *json = oas_doc_emit_json(doc, &opts, nullptr);
    TEST_ASSERT_NOT_NULL(json);

    /* Compact output has no newlines */
    TEST_ASSERT_NULL(strchr(json, '\n'));

    oas_emit_free(json);
}

void test_emitter_schema_string(void)
{
    oas_schema_t *s = make_schema(OAS_TYPE_STRING);
    s->min_length = 1;
    s->max_length = 255;
    s->pattern = "^[a-z]+$";

    char *json = oas_schema_emit_json(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(json);

    yyjson_doc *parsed = yyjson_read(json, strlen(json), 0);
    TEST_ASSERT_NOT_NULL(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    TEST_ASSERT_EQUAL_STRING("string", yyjson_get_str(yyjson_obj_get(root, "type")));
    TEST_ASSERT_EQUAL_INT(1, (int)yyjson_get_int(yyjson_obj_get(root, "minLength")));
    TEST_ASSERT_EQUAL_INT(255, (int)yyjson_get_int(yyjson_obj_get(root, "maxLength")));
    TEST_ASSERT_EQUAL_STRING("^[a-z]+$", yyjson_get_str(yyjson_obj_get(root, "pattern")));

    yyjson_doc_free(parsed);
    oas_emit_free(json);
}

void test_emitter_schema_object_nested(void)
{
    oas_schema_t *obj_schema = make_schema(OAS_TYPE_OBJECT);

    oas_schema_t *name_schema = make_schema(OAS_TYPE_STRING);
    oas_schema_t *age_schema = make_schema(OAS_TYPE_INTEGER);

    int rc = oas_schema_add_property(arena, obj_schema, "name", name_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);
    rc = oas_schema_add_property(arena, obj_schema, "age", age_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char *json = oas_schema_emit_json(obj_schema, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(json);

    yyjson_doc *parsed = yyjson_read(json, strlen(json), 0);
    TEST_ASSERT_NOT_NULL(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    yyjson_val *props = yyjson_obj_get(root, "properties");
    TEST_ASSERT_NOT_NULL(props);

    /* Both properties should exist */
    yyjson_val *name_val = yyjson_obj_get(props, "name");
    TEST_ASSERT_NOT_NULL(name_val);
    TEST_ASSERT_EQUAL_STRING("string", yyjson_get_str(yyjson_obj_get(name_val, "type")));

    yyjson_val *age_val = yyjson_obj_get(props, "age");
    TEST_ASSERT_NOT_NULL(age_val);
    TEST_ASSERT_EQUAL_STRING("integer", yyjson_get_str(yyjson_obj_get(age_val, "type")));

    yyjson_doc_free(parsed);
    oas_emit_free(json);
}

void test_emitter_ref_preserved(void)
{
    oas_schema_t *s = make_schema(0);
    s->ref = "#/components/schemas/Pet";

    char *json = oas_schema_emit_json(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(json);

    yyjson_doc *parsed = yyjson_read(json, strlen(json), 0);
    TEST_ASSERT_NOT_NULL(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    yyjson_val *ref = yyjson_obj_get(root, "$ref");
    TEST_ASSERT_NOT_NULL(ref);
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Pet", yyjson_get_str(ref));

    /* $ref should be the only key */
    TEST_ASSERT_EQUAL_UINT(1, yyjson_obj_size(root));

    yyjson_doc_free(parsed);
    oas_emit_free(json);
}

void test_emitter_roundtrip(void)
{
    oas_doc_t *doc = make_doc("Roundtrip", "2.0.0");

    oas_schema_t *str_schema = make_schema(OAS_TYPE_STRING);
    str_schema->format = "email";

    oas_schema_entry_t schema_entry = {.name = "Email", .schema = str_schema};

    oas_components_t *comp = oas_arena_alloc(arena, sizeof(*comp), _Alignof(oas_components_t));
    memset(comp, 0, sizeof(*comp));
    comp->schemas = &schema_entry;
    comp->schemas_count = 1;
    doc->components = comp;

    /* Emit first time */
    size_t len1 = 0;
    char *json1 = oas_doc_emit_json(doc, nullptr, &len1);
    TEST_ASSERT_NOT_NULL(json1);

    /* Parse emitted JSON and verify key fields */
    yyjson_doc *parsed = yyjson_read(json1, len1, 0);
    TEST_ASSERT_NOT_NULL(parsed);

    yyjson_val *root = yyjson_doc_get_root(parsed);
    TEST_ASSERT_EQUAL_STRING("3.2.0", yyjson_get_str(yyjson_obj_get(root, "openapi")));

    yyjson_val *info = yyjson_obj_get(root, "info");
    TEST_ASSERT_EQUAL_STRING("Roundtrip", yyjson_get_str(yyjson_obj_get(info, "title")));

    yyjson_val *schemas = yyjson_obj_get(yyjson_obj_get(root, "components"), "schemas");
    yyjson_val *email_schema = yyjson_obj_get(schemas, "Email");
    TEST_ASSERT_NOT_NULL(email_schema);
    TEST_ASSERT_EQUAL_STRING("string", yyjson_get_str(yyjson_obj_get(email_schema, "type")));
    TEST_ASSERT_EQUAL_STRING("email", yyjson_get_str(yyjson_obj_get(email_schema, "format")));

    yyjson_doc_free(parsed);
    oas_emit_free(json1);
}

void test_emitter_null_safe(void)
{
    /* nullptr doc returns nullptr */
    char *json = oas_doc_emit_json(nullptr, nullptr, nullptr);
    TEST_ASSERT_NULL(json);

    /* nullptr schema returns nullptr */
    json = oas_schema_emit_json(nullptr, nullptr, nullptr);
    TEST_ASSERT_NULL(json);

    /* oas_emit_free with nullptr is safe */
    oas_emit_free(nullptr);
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_emitter_minimal_doc);
    RUN_TEST(test_emitter_with_paths);
    RUN_TEST(test_emitter_with_components);
    RUN_TEST(test_emitter_pretty);
    RUN_TEST(test_emitter_compact);
    RUN_TEST(test_emitter_schema_string);
    RUN_TEST(test_emitter_schema_object_nested);
    RUN_TEST(test_emitter_ref_preserved);
    RUN_TEST(test_emitter_roundtrip);
    RUN_TEST(test_emitter_null_safe);
    return UNITY_END();
}
