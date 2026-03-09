#include "parser/oas_ref.h"

#include <liboas/oas_parser.h>

#include <errno.h>
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

/* Helper: parse a JSON string into doc + keep yyjson alive via arena context */
static const char *DOC_WITH_REFS = "{"
                                   "  \"openapi\": \"3.2.0\","
                                   "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                                   "  \"components\": {"
                                   "    \"schemas\": {"
                                   "      \"Pet\": {\"type\": \"object\", \"properties\": {"
                                   "        \"name\": {\"type\": \"string\"}"
                                   "      }},"
                                   "      \"Dog\": {\"allOf\": ["
                                   "        {\"$ref\": \"#/components/schemas/Pet\"},"
                                   "        {\"type\": \"object\", \"properties\": {"
                                   "          \"breed\": {\"type\": \"string\"}"
                                   "        }}"
                                   "      ]},"
                                   "      \"PetRef\": {\"$ref\": \"#/components/schemas/Pet\"}"
                                   "    }"
                                   "  }"
                                   "}";

/* Parse and return yyjson root (caller must free jdoc) */
static yyjson_doc *parse_json(const char *json)
{
    return yyjson_read(json, strlen(json), 0);
}

void test_ref_local_schema(void)
{
    yyjson_doc *jdoc = parse_json(DOC_WITH_REFS);
    TEST_ASSERT_NOT_NULL(jdoc);
    yyjson_val *root = yyjson_doc_get_root(jdoc);

    oas_ref_ctx_t *ctx = oas_ref_ctx_create(arena, root);
    TEST_ASSERT_NOT_NULL(ctx);

    yyjson_val *val = nullptr;
    int rc = oas_ref_resolve(ctx, "#/components/schemas/Pet", &val, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_TRUE(yyjson_is_obj(val));

    /* Verify it's the Pet schema */
    yyjson_val *type_val = yyjson_obj_get(val, "type");
    TEST_ASSERT_EQUAL_STRING("object", yyjson_get_str(type_val));

    yyjson_doc_free(jdoc);
}

void test_ref_local_parameter(void)
{
    const char *json = "{"
                       "  \"openapi\": \"3.2.0\","
                       "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                       "  \"components\": {"
                       "    \"schemas\": {"
                       "      \"Limit\": {\"type\": \"integer\", \"minimum\": 1}"
                       "    }"
                       "  }"
                       "}";
    yyjson_doc *jdoc = parse_json(json);
    yyjson_val *root = yyjson_doc_get_root(jdoc);

    oas_ref_ctx_t *ctx = oas_ref_ctx_create(arena, root);
    yyjson_val *val = nullptr;
    int rc = oas_ref_resolve(ctx, "#/components/schemas/Limit", &val, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("integer", yyjson_get_str(yyjson_obj_get(val, "type")));

    yyjson_doc_free(jdoc);
}

void test_ref_nested(void)
{
    /* A -> B -> C chain */
    const char *json = "{"
                       "  \"a\": {\"$ref\": \"#/b\"},"
                       "  \"b\": {\"$ref\": \"#/c\"},"
                       "  \"c\": {\"type\": \"string\"}"
                       "}";
    yyjson_doc *jdoc = parse_json(json);
    yyjson_val *root = yyjson_doc_get_root(jdoc);

    oas_ref_ctx_t *ctx = oas_ref_ctx_create(arena, root);
    yyjson_val *val = nullptr;
    int rc = oas_ref_resolve(ctx, "#/a", &val, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(val);
    /* Should resolve to the final target {type: string} */
    TEST_ASSERT_EQUAL_STRING("string", yyjson_get_str(yyjson_obj_get(val, "type")));

    yyjson_doc_free(jdoc);
}

void test_ref_cycle_detection(void)
{
    /* A -> B -> A -> cycle! */
    const char *json = "{"
                       "  \"a\": {\"$ref\": \"#/b\"},"
                       "  \"b\": {\"$ref\": \"#/a\"}"
                       "}";
    yyjson_doc *jdoc = parse_json(json);
    yyjson_val *root = yyjson_doc_get_root(jdoc);

    oas_ref_ctx_t *ctx = oas_ref_ctx_create(arena, root);
    yyjson_val *val = nullptr;
    int rc = oas_ref_resolve(ctx, "#/a", &val, errors);
    TEST_ASSERT_EQUAL_INT(-ELOOP, rc);
    TEST_ASSERT_NULL(val);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));

    yyjson_doc_free(jdoc);
}

void test_ref_self_reference(void)
{
    /* A -> A (self-reference) */
    const char *json = "{"
                       "  \"a\": {\"$ref\": \"#/a\"}"
                       "}";
    yyjson_doc *jdoc = parse_json(json);
    yyjson_val *root = yyjson_doc_get_root(jdoc);

    oas_ref_ctx_t *ctx = oas_ref_ctx_create(arena, root);
    yyjson_val *val = nullptr;
    int rc = oas_ref_resolve(ctx, "#/a", &val, errors);
    TEST_ASSERT_EQUAL_INT(-ELOOP, rc);

    yyjson_doc_free(jdoc);
}

void test_ref_missing_target(void)
{
    const char *json = "{\"a\": 1}";
    yyjson_doc *jdoc = parse_json(json);
    yyjson_val *root = yyjson_doc_get_root(jdoc);

    oas_ref_ctx_t *ctx = oas_ref_ctx_create(arena, root);
    yyjson_val *val = nullptr;
    int rc = oas_ref_resolve(ctx, "#/nonexistent", &val, errors);
    TEST_ASSERT_EQUAL_INT(-ENOENT, rc);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));

    yyjson_doc_free(jdoc);
}

void test_ref_resolve_all_schemas(void)
{
    oas_doc_t *doc = oas_doc_parse(arena, DOC_WITH_REFS, strlen(DOC_WITH_REFS), errors);
    TEST_ASSERT_NOT_NULL_MESSAGE(doc, "DOC_WITH_REFS should parse");
    TEST_ASSERT_NOT_NULL(doc->components);
    TEST_ASSERT_EQUAL_UINT64(3, doc->components->schemas_count);

    /* Parse the raw JSON for ref context */
    yyjson_doc *jdoc = parse_json(DOC_WITH_REFS);
    yyjson_val *root = yyjson_doc_get_root(jdoc);
    oas_ref_ctx_t *ctx = oas_ref_ctx_create(arena, root);

    int rc = oas_ref_resolve_all(ctx, doc, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* PetRef should have ref_resolved pointing to Pet */
    oas_schema_t *pet_ref_schema = nullptr;
    oas_schema_t *pet_schema = nullptr;
    for (size_t i = 0; i < doc->components->schemas_count; i++) {
        if (strcmp(doc->components->schemas[i].name, "PetRef") == 0) {
            pet_ref_schema = doc->components->schemas[i].schema;
        }
        if (strcmp(doc->components->schemas[i].name, "Pet") == 0) {
            pet_schema = doc->components->schemas[i].schema;
        }
    }
    TEST_ASSERT_NOT_NULL(pet_ref_schema);
    TEST_ASSERT_NOT_NULL(pet_schema);
    TEST_ASSERT_NOT_NULL(pet_ref_schema->ref_resolved);
    TEST_ASSERT_EQUAL_PTR(pet_schema, pet_ref_schema->ref_resolved);

    yyjson_doc_free(jdoc);
    oas_doc_free(doc);
}

void test_ref_fragment_only(void)
{
    const char *json = "{"
                       "  \"definitions\": {"
                       "    \"Foo\": {\"type\": \"number\"}"
                       "  }"
                       "}";
    yyjson_doc *jdoc = parse_json(json);
    yyjson_val *root = yyjson_doc_get_root(jdoc);

    oas_ref_ctx_t *ctx = oas_ref_ctx_create(arena, root);
    yyjson_val *val = nullptr;
    int rc = oas_ref_resolve(ctx, "#/definitions/Foo", &val, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("number", yyjson_get_str(yyjson_obj_get(val, "type")));

    yyjson_doc_free(jdoc);
}

void test_ref_invalid_pointer(void)
{
    const char *json = "{\"a\": 1}";
    yyjson_doc *jdoc = parse_json(json);
    yyjson_val *root = yyjson_doc_get_root(jdoc);

    oas_ref_ctx_t *ctx = oas_ref_ctx_create(arena, root);
    yyjson_val *val = nullptr;

    /* Malformed: missing leading / after # */
    int rc = oas_ref_resolve(ctx, "#missing-slash", &val, errors);
    TEST_ASSERT_EQUAL_INT(-ENOENT, rc);

    yyjson_doc_free(jdoc);
}

void test_ref_external_not_supported(void)
{
    const char *json = "{\"a\": 1}";
    yyjson_doc *jdoc = parse_json(json);
    yyjson_val *root = yyjson_doc_get_root(jdoc);

    oas_ref_ctx_t *ctx = oas_ref_ctx_create(arena, root);
    yyjson_val *val = nullptr;
    int rc = oas_ref_resolve(ctx, "https://example.com/schemas/Pet.json", &val, errors);
    TEST_ASSERT_EQUAL_INT(-ENOTSUP, rc);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));

    yyjson_doc_free(jdoc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ref_local_schema);
    RUN_TEST(test_ref_local_parameter);
    RUN_TEST(test_ref_nested);
    RUN_TEST(test_ref_cycle_detection);
    RUN_TEST(test_ref_self_reference);
    RUN_TEST(test_ref_missing_target);
    RUN_TEST(test_ref_resolve_all_schemas);
    RUN_TEST(test_ref_fragment_only);
    RUN_TEST(test_ref_invalid_pointer);
    RUN_TEST(test_ref_external_not_supported);
    return UNITY_END();
}
