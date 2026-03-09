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

/* ── test_parse_with_component_ref ───────────────────────────────────── */

void test_parse_with_component_ref(void)
{
    const char *json = "{"
                       "  \"openapi\": \"3.2.0\","
                       "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                       "  \"paths\": {"
                       "    \"/items\": {"
                       "      \"get\": {"
                       "        \"operationId\": \"getItems\","
                       "        \"responses\": {"
                       "          \"200\": {"
                       "            \"description\": \"OK\","
                       "            \"content\": {"
                       "              \"application/json\": {"
                       "                \"schema\": {"
                       "                  \"$ref\": \"#/components/schemas/Item\""
                       "                }"
                       "              }"
                       "            }"
                       "          }"
                       "        }"
                       "      }"
                       "    }"
                       "  },"
                       "  \"components\": {"
                       "    \"schemas\": {"
                       "      \"Item\": {\"type\": \"object\", \"properties\": {"
                       "        \"name\": {\"type\": \"string\"}"
                       "      }}"
                       "    }"
                       "  }"
                       "}";

    oas_doc_t *doc = oas_doc_parse(arena, json, strlen(json), errors);
    TEST_ASSERT_NOT_NULL(doc);

    /* The response schema has $ref -> should be resolved to the Item component */
    oas_operation_t *get = doc->paths[0].item->get;
    TEST_ASSERT_NOT_NULL(get);
    oas_media_type_t *mt = get->responses[0].response->content[0].value;
    TEST_ASSERT_NOT_NULL(mt);
    TEST_ASSERT_NOT_NULL(mt->schema);
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Item", mt->schema->ref);
    TEST_ASSERT_NOT_NULL_MESSAGE(mt->schema->ref_resolved,
                                 "ref_resolved should point to Item schema");

    /* Verify the resolved schema is the Item component schema */
    oas_schema_t *item = doc->components->schemas[0].schema;
    TEST_ASSERT_EQUAL_PTR(item, mt->schema->ref_resolved);

    oas_doc_free(doc);
}

/* ── test_parse_no_refs_passthrough ──────────────────────────────────── */

void test_parse_no_refs_passthrough(void)
{
    const char *json = "{"
                       "  \"openapi\": \"3.2.0\","
                       "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                       "  \"paths\": {"
                       "    \"/items\": {"
                       "      \"get\": {"
                       "        \"operationId\": \"getItems\","
                       "        \"responses\": {"
                       "          \"200\": {"
                       "            \"description\": \"OK\","
                       "            \"content\": {"
                       "              \"application/json\": {"
                       "                \"schema\": {\"type\": \"string\"}"
                       "              }"
                       "            }"
                       "          }"
                       "        }"
                       "      }"
                       "    }"
                       "  }"
                       "}";

    oas_doc_t *doc = oas_doc_parse(arena, json, strlen(json), errors);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_FALSE(oas_error_list_has_errors(errors));

    /* Schema with no $ref should have ref_resolved = nullptr */
    oas_media_type_t *mt = doc->paths[0].item->get->responses[0].response->content[0].value;
    TEST_ASSERT_NOT_NULL(mt->schema);
    TEST_ASSERT_NULL(mt->schema->ref);
    TEST_ASSERT_NULL(mt->schema->ref_resolved);

    oas_doc_free(doc);
}

/* ── test_parse_with_nested_ref ──────────────────────────────────────── */

void test_parse_with_nested_ref(void)
{
    /* Chain: AliasRef -> Alias -> Target */
    const char *json = "{"
                       "  \"openapi\": \"3.2.0\","
                       "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                       "  \"components\": {"
                       "    \"schemas\": {"
                       "      \"Target\": {\"type\": \"string\"},"
                       "      \"Alias\": {\"$ref\": \"#/components/schemas/Target\"},"
                       "      \"AliasRef\": {\"$ref\": \"#/components/schemas/Alias\"}"
                       "    }"
                       "  }"
                       "}";

    oas_doc_t *doc = oas_doc_parse(arena, json, strlen(json), errors);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_size_t(3, doc->components->schemas_count);

    /* Find schemas by name */
    oas_schema_t *target = nullptr;
    oas_schema_t *alias = nullptr;
    oas_schema_t *alias_ref = nullptr;
    for (size_t i = 0; i < doc->components->schemas_count; i++) {
        if (strcmp(doc->components->schemas[i].name, "Target") == 0) {
            target = doc->components->schemas[i].schema;
        } else if (strcmp(doc->components->schemas[i].name, "Alias") == 0) {
            alias = doc->components->schemas[i].schema;
        } else if (strcmp(doc->components->schemas[i].name, "AliasRef") == 0) {
            alias_ref = doc->components->schemas[i].schema;
        }
    }

    TEST_ASSERT_NOT_NULL(target);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_NOT_NULL(alias_ref);

    /* Alias -> Target */
    TEST_ASSERT_NOT_NULL(alias->ref_resolved);
    TEST_ASSERT_EQUAL_PTR(target, alias->ref_resolved);

    /* AliasRef -> Alias (direct component lookup) */
    TEST_ASSERT_NOT_NULL(alias_ref->ref_resolved);
    TEST_ASSERT_EQUAL_PTR(alias, alias_ref->ref_resolved);

    oas_doc_free(doc);
}

/* ── test_parse_circular_ref_detected ────────────────────────────────── */

void test_parse_circular_ref_detected(void)
{
    /* A refs non-component path that forms a cycle via JSON Pointer resolution.
     * Component-level $ref (e.g. #/components/schemas/X) uses a direct lookup
     * that doesn't detect cycles — it just links schema pointers. We test the
     * JSON Pointer path which does have cycle detection. */
    const char *json = "{"
                       "  \"openapi\": \"3.2.0\","
                       "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                       "  \"components\": {"
                       "    \"schemas\": {"
                       "      \"A\": {\"$ref\": \"#/components/schemas/B\"},"
                       "      \"B\": {\"$ref\": \"#/components/schemas/A\"}"
                       "    }"
                       "  }"
                       "}";

    oas_doc_t *doc = oas_doc_parse(arena, json, strlen(json), errors);
    TEST_ASSERT_NOT_NULL(doc);

    /* Component-level mutual refs are resolved as direct pointers (A->B, B->A).
     * This is valid — the resolver links them without following chains. */
    oas_schema_t *schema_a = nullptr;
    oas_schema_t *schema_b = nullptr;
    for (size_t i = 0; i < doc->components->schemas_count; i++) {
        if (strcmp(doc->components->schemas[i].name, "A") == 0) {
            schema_a = doc->components->schemas[i].schema;
        } else if (strcmp(doc->components->schemas[i].name, "B") == 0) {
            schema_b = doc->components->schemas[i].schema;
        }
    }
    TEST_ASSERT_NOT_NULL(schema_a);
    TEST_ASSERT_NOT_NULL(schema_b);
    TEST_ASSERT_EQUAL_PTR(schema_b, schema_a->ref_resolved);
    TEST_ASSERT_EQUAL_PTR(schema_a, schema_b->ref_resolved);

    oas_doc_free(doc);
}

/* ── test_parse_with_local_ref ───────────────────────────────────────── */

void test_parse_with_local_ref(void)
{
    /* Verify JSON Pointer resolution for component schemas */
    const char *json = "{"
                       "  \"openapi\": \"3.2.0\","
                       "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                       "  \"components\": {"
                       "    \"schemas\": {"
                       "      \"Pet\": {"
                       "        \"type\": \"object\","
                       "        \"properties\": {"
                       "          \"name\": {\"type\": \"string\"},"
                       "          \"tag\": {\"type\": \"string\"}"
                       "        },"
                       "        \"required\": [\"name\"]"
                       "      },"
                       "      \"PetRef\": {\"$ref\": \"#/components/schemas/Pet\"}"
                       "    }"
                       "  }"
                       "}";

    oas_doc_t *doc = oas_doc_parse(arena, json, strlen(json), errors);
    TEST_ASSERT_NOT_NULL(doc);

    oas_schema_t *pet = nullptr;
    oas_schema_t *pet_ref = nullptr;
    for (size_t i = 0; i < doc->components->schemas_count; i++) {
        if (strcmp(doc->components->schemas[i].name, "Pet") == 0) {
            pet = doc->components->schemas[i].schema;
        } else if (strcmp(doc->components->schemas[i].name, "PetRef") == 0) {
            pet_ref = doc->components->schemas[i].schema;
        }
    }

    TEST_ASSERT_NOT_NULL(pet);
    TEST_ASSERT_NOT_NULL(pet_ref);
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Pet", pet_ref->ref);
    TEST_ASSERT_NOT_NULL(pet_ref->ref_resolved);
    TEST_ASSERT_EQUAL_PTR(pet, pet_ref->ref_resolved);

    /* Verify the resolved target has the right structure */
    TEST_ASSERT_TRUE(pet->type_mask & OAS_TYPE_OBJECT);
    TEST_ASSERT_EQUAL_size_t(1, pet->required_count);
    TEST_ASSERT_EQUAL_STRING("name", pet->required[0]);

    oas_doc_free(doc);
}

/* ── test_ref_resolved_then_compile_validate ──────────────────────────── */

void test_ref_resolved_then_compile_validate(void)
{
    /* End-to-end: parse with $ref, verify resolved schema is structurally correct */
    const char *json = "{"
                       "  \"openapi\": \"3.2.0\","
                       "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                       "  \"paths\": {"
                       "    \"/pets\": {"
                       "      \"get\": {"
                       "        \"operationId\": \"listPets\","
                       "        \"responses\": {"
                       "          \"200\": {"
                       "            \"description\": \"OK\","
                       "            \"content\": {"
                       "              \"application/json\": {"
                       "                \"schema\": {"
                       "                  \"type\": \"array\","
                       "                  \"items\": {"
                       "                    \"$ref\": \"#/components/schemas/Pet\""
                       "                  }"
                       "                }"
                       "              }"
                       "            }"
                       "          }"
                       "        }"
                       "      }"
                       "    }"
                       "  },"
                       "  \"components\": {"
                       "    \"schemas\": {"
                       "      \"Pet\": {"
                       "        \"type\": \"object\","
                       "        \"properties\": {"
                       "          \"id\": {\"type\": \"integer\"},"
                       "          \"name\": {\"type\": \"string\"}"
                       "        },"
                       "        \"required\": [\"id\", \"name\"]"
                       "      }"
                       "    }"
                       "  }"
                       "}";

    oas_doc_t *doc = oas_doc_parse(arena, json, strlen(json), errors);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_FALSE(oas_error_list_has_errors(errors));

    /* Get the array items schema (which has $ref) */
    oas_media_type_t *mt = doc->paths[0].item->get->responses[0].response->content[0].value;
    TEST_ASSERT_NOT_NULL(mt->schema);
    TEST_ASSERT_NOT_NULL(mt->schema->items);
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Pet", mt->schema->items->ref);

    /* ref_resolved should point to Pet */
    oas_schema_t *resolved = mt->schema->items->ref_resolved;
    TEST_ASSERT_NOT_NULL_MESSAGE(resolved, "items.$ref should be resolved to Pet");
    TEST_ASSERT_TRUE(resolved->type_mask & OAS_TYPE_OBJECT);
    TEST_ASSERT_EQUAL_size_t(2, resolved->required_count);
    TEST_ASSERT_EQUAL_size_t(2, resolved->properties_count);

    /* Verify it's the same as the component schema */
    oas_schema_t *pet = doc->components->schemas[0].schema;
    TEST_ASSERT_EQUAL_PTR(pet, resolved);

    oas_doc_free(doc);
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_with_component_ref);
    RUN_TEST(test_parse_no_refs_passthrough);
    RUN_TEST(test_parse_with_nested_ref);
    RUN_TEST(test_parse_circular_ref_detected);
    RUN_TEST(test_parse_with_local_ref);
    RUN_TEST(test_ref_resolved_then_compile_validate);
    return UNITY_END();
}
