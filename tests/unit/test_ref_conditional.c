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

/* ── Test 1: $ref in not_schema resolves correctly ────────────────────── */

void test_ref_in_not_resolved(void)
{
    const char *json = "{"
                       "  \"openapi\": \"3.2.0\","
                       "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                       "  \"components\": {"
                       "    \"schemas\": {"
                       "      \"Name\": {\"type\": \"string\"},"
                       "      \"NotName\": {\"not\": {\"$ref\": \"#/components/schemas/Name\"}}"
                       "    }"
                       "  }"
                       "}";

    oas_doc_t *doc = oas_doc_parse(arena, json, strlen(json), errors);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_NOT_NULL(doc->components);
    TEST_ASSERT_TRUE(doc->components->schemas_count >= 2);

    /* Find NotName schema */
    oas_schema_t *not_name = nullptr;
    for (size_t i = 0; i < doc->components->schemas_count; i++) {
        if (strcmp(doc->components->schemas[i].name, "NotName") == 0) {
            not_name = doc->components->schemas[i].schema;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(not_name);
    TEST_ASSERT_NOT_NULL(not_name->not_schema);
    TEST_ASSERT_NOT_NULL(not_name->not_schema->ref_resolved);

    oas_doc_free(doc);
}

/* ── Test 2: $ref in if/then/else resolves correctly ──────────────────── */

void test_ref_in_if_then_else(void)
{
    const char *json = "{"
                       "  \"openapi\": \"3.2.0\","
                       "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                       "  \"components\": {"
                       "    \"schemas\": {"
                       "      \"Name\": {\"type\": \"string\"},"
                       "      \"Age\": {\"type\": \"integer\"},"
                       "      \"Flag\": {\"type\": \"boolean\"},"
                       "      \"Cond\": {"
                       "        \"if\": {\"$ref\": \"#/components/schemas/Flag\"},"
                       "        \"then\": {\"$ref\": \"#/components/schemas/Name\"},"
                       "        \"else\": {\"$ref\": \"#/components/schemas/Age\"}"
                       "      }"
                       "    }"
                       "  }"
                       "}";

    oas_doc_t *doc = oas_doc_parse(arena, json, strlen(json), errors);
    TEST_ASSERT_NOT_NULL(doc);

    oas_schema_t *cond = nullptr;
    for (size_t i = 0; i < doc->components->schemas_count; i++) {
        if (strcmp(doc->components->schemas[i].name, "Cond") == 0) {
            cond = doc->components->schemas[i].schema;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(cond);
    TEST_ASSERT_NOT_NULL(cond->if_schema);
    TEST_ASSERT_NOT_NULL(cond->if_schema->ref_resolved);
    TEST_ASSERT_NOT_NULL(cond->then_schema);
    TEST_ASSERT_NOT_NULL(cond->then_schema->ref_resolved);
    TEST_ASSERT_NOT_NULL(cond->else_schema);
    TEST_ASSERT_NOT_NULL(cond->else_schema->ref_resolved);

    oas_doc_free(doc);
}

/* ── Test 3: bad $ref in not_schema returns error ─────────────────────── */

void test_ref_in_not_bad_ref(void)
{
    const char *json = "{"
                       "  \"openapi\": \"3.2.0\","
                       "  \"info\": {\"title\": \"T\", \"version\": \"1\"},"
                       "  \"components\": {"
                       "    \"schemas\": {"
                       "      \"Bad\": {\"not\": {\"$ref\": \"#/components/schemas/Nonexistent\"}}"
                       "    }"
                       "  }"
                       "}";

    oas_doc_t *doc = oas_doc_parse(arena, json, strlen(json), errors);
    /* Doc may still be returned (errors accumulated), but errors should be present */
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));

    if (doc) {
        oas_doc_free(doc);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ref_in_not_resolved);
    RUN_TEST(test_ref_in_if_then_else);
    RUN_TEST(test_ref_in_not_bad_ref);
    return UNITY_END();
}
