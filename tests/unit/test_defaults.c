#include <liboas/oas_compiler.h>
#include <liboas/oas_doc.h>
#include <liboas/oas_error.h>
#include <liboas/oas_schema.h>
#include <liboas/oas_validator.h>

#include <stdlib.h>
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

/* Helper: build a minimal doc with one POST path and a request body schema */
static oas_doc_t *build_test_doc(const char *path, oas_schema_t *req_body_schema)
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
    op->operation_id = "testOp";

    oas_request_body_t *rb = oas_arena_alloc(arena, sizeof(*rb), _Alignof(oas_request_body_t));
    TEST_ASSERT_NOT_NULL(rb);
    memset(rb, 0, sizeof(*rb));
    rb->required = false;

    oas_media_type_t *mt = oas_arena_alloc(arena, sizeof(*mt), _Alignof(oas_media_type_t));
    TEST_ASSERT_NOT_NULL(mt);
    mt->media_type_name = "application/json";
    mt->schema = req_body_schema;

    oas_media_type_entry_t *mte =
        oas_arena_alloc(arena, sizeof(*mte), _Alignof(oas_media_type_entry_t));
    TEST_ASSERT_NOT_NULL(mte);
    mte->key = "application/json";
    mte->value = mt;

    rb->content = mte;
    rb->content_count = 1;
    op->request_body = rb;

    oas_path_item_t *item = oas_arena_alloc(arena, sizeof(*item), _Alignof(oas_path_item_t));
    TEST_ASSERT_NOT_NULL(item);
    memset(item, 0, sizeof(*item));
    item->path = path;
    item->post = op;

    oas_path_entry_t *pe = oas_arena_alloc(arena, sizeof(*pe), _Alignof(oas_path_entry_t));
    TEST_ASSERT_NOT_NULL(pe);
    pe->path = path;
    pe->item = item;

    doc->paths = pe;
    doc->paths_count = 1;

    return doc;
}

/* Helper: parse a yyjson_val from a JSON string, keep the doc alive in arena storage */
static yyjson_val *parse_json_val(const char *json)
{
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    TEST_ASSERT_NOT_NULL(doc);

    /* Store the doc pointer in arena so we can find it later for cleanup
     * In practice, the yyjson_val is valid as long as doc lives.
     * We leak the doc intentionally — arena tearDown handles the test scope. */
    yyjson_doc **slot = oas_arena_alloc(arena, sizeof(*slot), _Alignof(yyjson_doc *));
    TEST_ASSERT_NOT_NULL(slot);
    *slot = doc;

    return yyjson_doc_get_root(doc);
}

/* ── Test 1: default applied for missing field ─────────────────────────── */

void test_default_applied_missing_field(void)
{
    oas_schema_t *schema = oas_schema_create(arena);
    schema->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *name_schema = oas_schema_create(arena);
    name_schema->type_mask = OAS_TYPE_STRING;

    oas_schema_t *status_schema = oas_schema_create(arena);
    status_schema->type_mask = OAS_TYPE_STRING;
    status_schema->default_value = parse_json_val("\"active\"");

    int rc = oas_schema_add_property(arena, schema, "name", name_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);
    rc = oas_schema_add_property(arena, schema, "status", status_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* Apply defaults to a body missing "status" */
    const char *body = "{\"name\":\"Fido\"}";
    char *out = nullptr;
    size_t out_len = 0;

    rc = oas_apply_defaults(schema, body, strlen(body), &out, &out_len);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(out);

    /* Parse output and check that "status" was added */
    yyjson_doc *odoc = yyjson_read(out, out_len, 0);
    TEST_ASSERT_NOT_NULL(odoc);
    yyjson_val *root = yyjson_doc_get_root(odoc);
    TEST_ASSERT_TRUE(yyjson_is_obj(root));

    yyjson_val *name_val = yyjson_obj_get(root, "name");
    TEST_ASSERT_NOT_NULL(name_val);
    TEST_ASSERT_EQUAL_STRING("Fido", yyjson_get_str(name_val));

    yyjson_val *status_val = yyjson_obj_get(root, "status");
    TEST_ASSERT_NOT_NULL(status_val);
    TEST_ASSERT_EQUAL_STRING("active", yyjson_get_str(status_val));

    yyjson_doc_free(odoc);
    free(out);
}

/* ── Test 2: default does not overwrite existing field ──────────────────── */

void test_default_not_overwrite_existing(void)
{
    oas_schema_t *schema = oas_schema_create(arena);
    schema->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *status_schema = oas_schema_create(arena);
    status_schema->type_mask = OAS_TYPE_STRING;
    status_schema->default_value = parse_json_val("\"active\"");

    int rc = oas_schema_add_property(arena, schema, "status", status_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* Body already has "status" — should not be overwritten */
    const char *body = "{\"status\":\"inactive\"}";
    char *out = nullptr;
    size_t out_len = 0;

    rc = oas_apply_defaults(schema, body, strlen(body), &out, &out_len);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(out);

    yyjson_doc *odoc = yyjson_read(out, out_len, 0);
    TEST_ASSERT_NOT_NULL(odoc);
    yyjson_val *root = yyjson_doc_get_root(odoc);
    yyjson_val *status_val = yyjson_obj_get(root, "status");
    TEST_ASSERT_NOT_NULL(status_val);
    TEST_ASSERT_EQUAL_STRING("inactive", yyjson_get_str(status_val));

    yyjson_doc_free(odoc);
    free(out);
}

/* ── Test 3: default applied in nested objects ─────────────────────────── */

void test_default_nested_object(void)
{
    oas_schema_t *schema = oas_schema_create(arena);
    schema->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *address_schema = oas_schema_create(arena);
    address_schema->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *country_schema = oas_schema_create(arena);
    country_schema->type_mask = OAS_TYPE_STRING;
    country_schema->default_value = parse_json_val("\"US\"");

    int rc = oas_schema_add_property(arena, address_schema, "country", country_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);
    rc = oas_schema_add_property(arena, schema, "address", address_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* Nested object missing "country" */
    const char *body = "{\"address\":{\"city\":\"NYC\"}}";
    char *out = nullptr;
    size_t out_len = 0;

    rc = oas_apply_defaults(schema, body, strlen(body), &out, &out_len);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(out);

    yyjson_doc *odoc = yyjson_read(out, out_len, 0);
    TEST_ASSERT_NOT_NULL(odoc);
    yyjson_val *root = yyjson_doc_get_root(odoc);
    yyjson_val *addr = yyjson_obj_get(root, "address");
    TEST_ASSERT_NOT_NULL(addr);
    TEST_ASSERT_TRUE(yyjson_is_obj(addr));

    yyjson_val *country = yyjson_obj_get(addr, "country");
    TEST_ASSERT_NOT_NULL(country);
    TEST_ASSERT_EQUAL_STRING("US", yyjson_get_str(country));

    yyjson_val *city = yyjson_obj_get(addr, "city");
    TEST_ASSERT_NOT_NULL(city);
    TEST_ASSERT_EQUAL_STRING("NYC", yyjson_get_str(city));

    yyjson_doc_free(odoc);
    free(out);
}

/* ── Test 4: array items with default are not applied at array level ─── */

void test_default_array_items(void)
{
    oas_schema_t *schema = oas_schema_create(arena);
    schema->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *tags_schema = oas_schema_create(arena);
    tags_schema->type_mask = OAS_TYPE_ARRAY;
    tags_schema->default_value = parse_json_val("[\"general\"]");

    oas_schema_t *items_schema = oas_schema_create(arena);
    items_schema->type_mask = OAS_TYPE_STRING;
    tags_schema->items = items_schema;

    int rc = oas_schema_add_property(arena, schema, "tags", tags_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* Body missing "tags" — default array should be applied */
    const char *body = "{\"name\":\"test\"}";
    char *out = nullptr;
    size_t out_len = 0;

    rc = oas_apply_defaults(schema, body, strlen(body), &out, &out_len);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(out);

    yyjson_doc *odoc = yyjson_read(out, out_len, 0);
    TEST_ASSERT_NOT_NULL(odoc);
    yyjson_val *root = yyjson_doc_get_root(odoc);
    yyjson_val *tags = yyjson_obj_get(root, "tags");
    TEST_ASSERT_NOT_NULL(tags);
    TEST_ASSERT_TRUE(yyjson_is_arr(tags));
    TEST_ASSERT_EQUAL_size_t(1, yyjson_arr_size(tags));
    TEST_ASSERT_EQUAL_STRING("general", yyjson_get_str(yyjson_arr_get_first(tags)));

    yyjson_doc_free(odoc);
    free(out);
}

/* ── Test 5: defaults still returned when validation fails ─────────────── */

void test_default_no_mutation_on_fail(void)
{
    oas_schema_t *schema = oas_schema_create(arena);
    schema->type_mask = OAS_TYPE_OBJECT;

    static const char *required[] = {"name"};
    schema->required = (const char **)required;
    schema->required_count = 1;

    oas_schema_t *name_schema = oas_schema_create(arena);
    name_schema->type_mask = OAS_TYPE_STRING;
    int rc = oas_schema_add_property(arena, schema, "name", name_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_schema_t *status_schema = oas_schema_create(arena);
    status_schema->type_mask = OAS_TYPE_STRING;
    status_schema->default_value = parse_json_val("\"active\"");
    rc = oas_schema_add_property(arena, schema, "status", status_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_doc_t *doc = build_test_doc("/pets", schema);

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cdoc);

    /* Body missing required "name" — validation should fail,
     * but defaults should still be applied in out_body */
    const char *body = "{\"age\":5}";
    oas_http_request_t req = {
        .method = "POST",
        .path = "/pets",
        .content_type = "application/json",
        .body = body,
        .body_len = strlen(body),
    };

    oas_validation_result_t result = {0};
    char *out = nullptr;
    size_t out_len = 0;
    rc = oas_validate_request_with_defaults(cdoc, &req, &result, arena, &out, &out_len);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(result.valid);

    /* Defaults should still be applied in the output */
    TEST_ASSERT_NOT_NULL(out);
    yyjson_doc *odoc = yyjson_read(out, out_len, 0);
    TEST_ASSERT_NOT_NULL(odoc);
    yyjson_val *root = yyjson_doc_get_root(odoc);
    yyjson_val *status_val = yyjson_obj_get(root, "status");
    TEST_ASSERT_NOT_NULL(status_val);
    TEST_ASSERT_EQUAL_STRING("active", yyjson_get_str(status_val));

    yyjson_doc_free(odoc);
    free(out);
    oas_compiled_doc_free(cdoc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_applied_missing_field);
    RUN_TEST(test_default_not_overwrite_existing);
    RUN_TEST(test_default_nested_object);
    RUN_TEST(test_default_array_items);
    RUN_TEST(test_default_no_mutation_on_fail);
    return UNITY_END();
}
