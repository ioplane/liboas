#include <liboas/oas_compiler.h>
#include <liboas/oas_schema.h>
#include <liboas/oas_validator.h>

#include "compiler/oas_format.h"
#include "compiler/oas_instruction.h"

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

/* Helper: compile a schema with default config */
static oas_compiled_schema_t *compile_schema(oas_schema_t *s)
{
    oas_compiler_config_t config = {.regex = nullptr, .format_policy = OAS_FORMAT_IGNORE};
    oas_compiled_schema_t *cs = oas_schema_compile(s, &config, nullptr);
    TEST_ASSERT_NOT_NULL(cs);
    return cs;
}

/* Helper: validate JSON with direction */
static oas_validation_result_t validate_dir(oas_compiled_schema_t *cs, const char *json,
                                            oas_validation_direction_t dir)
{
    oas_validation_result_t result = {0};
    int rc = oas_validate_json_with_direction(cs, json, strlen(json), dir, &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    return result;
}

/* ── Test 1: readOnly field rejected in request ──────────────────────── */

void test_readOnly_rejected_in_request(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *id_schema = oas_schema_create(arena);
    id_schema->type_mask = OAS_TYPE_INTEGER;
    id_schema->read_only = true;
    int rc = oas_schema_add_property(arena, s, "id", id_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_compiled_schema_t *cs = compile_schema(s);

    /* readOnly field present in request body — should fail */
    oas_validation_result_t r = validate_dir(cs, "{\"id\":42}", OAS_DIR_REQUEST);
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 2: readOnly field allowed in response ──────────────────────── */

void test_readOnly_allowed_in_response(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *id_schema = oas_schema_create(arena);
    id_schema->type_mask = OAS_TYPE_INTEGER;
    id_schema->read_only = true;
    int rc = oas_schema_add_property(arena, s, "id", id_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_compiled_schema_t *cs = compile_schema(s);

    /* readOnly field in response — should pass */
    oas_validation_result_t r = validate_dir(cs, "{\"id\":42}", OAS_DIR_RESPONSE);
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 3: writeOnly field rejected in response ────────────────────── */

void test_writeOnly_rejected_in_response(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *pw_schema = oas_schema_create(arena);
    pw_schema->type_mask = OAS_TYPE_STRING;
    pw_schema->write_only = true;
    int rc = oas_schema_add_property(arena, s, "password", pw_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_compiled_schema_t *cs = compile_schema(s);

    /* writeOnly field present in response body — should fail */
    oas_validation_result_t r = validate_dir(cs, "{\"password\":\"secret\"}", OAS_DIR_RESPONSE);
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 4: writeOnly field allowed in request ──────────────────────── */

void test_writeOnly_allowed_in_request(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *pw_schema = oas_schema_create(arena);
    pw_schema->type_mask = OAS_TYPE_STRING;
    pw_schema->write_only = true;
    int rc = oas_schema_add_property(arena, s, "password", pw_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_compiled_schema_t *cs = compile_schema(s);

    /* writeOnly field in request — should pass */
    oas_validation_result_t r = validate_dir(cs, "{\"password\":\"secret\"}", OAS_DIR_REQUEST);
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 5: readOnly ignored in standalone validation ───────────────── */

void test_readOnly_ignored_standalone(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *id_schema = oas_schema_create(arena);
    id_schema->type_mask = OAS_TYPE_INTEGER;
    id_schema->read_only = true;
    int rc = oas_schema_add_property(arena, s, "id", id_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_compiled_schema_t *cs = compile_schema(s);

    /* DIR_NONE: readOnly/writeOnly checks are ignored */
    oas_validation_result_t r = validate_dir(cs, "{\"id\":42}", OAS_DIR_NONE);
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 6: readOnly inside allOf composition ───────────────────────── */

void test_nested_readOnly_in_allOf(void)
{
    /* Build: allOf[ { properties: { id: { type: integer, readOnly: true } } } ] */
    oas_schema_t *root = oas_schema_create(arena);

    oas_schema_t *branch = oas_schema_create(arena);
    branch->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *id_schema = oas_schema_create(arena);
    id_schema->type_mask = OAS_TYPE_INTEGER;
    id_schema->read_only = true;
    int rc = oas_schema_add_property(arena, branch, "id", id_schema);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_schema_t *branches[1] = {branch};
    root->all_of = branches; //-V507
    root->all_of_count = 1;

    oas_compiled_schema_t *cs = compile_schema(root);

    /* readOnly inside allOf still rejected in request */
    oas_validation_result_t r1 = validate_dir(cs, "{\"id\":42}", OAS_DIR_REQUEST);
    TEST_ASSERT_FALSE(r1.valid);

    /* but allowed in response */
    oas_validation_result_t r2 = validate_dir(cs, "{\"id\":42}", OAS_DIR_RESPONSE);
    TEST_ASSERT_TRUE(r2.valid);

    oas_compiled_schema_free(cs);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_readOnly_rejected_in_request);
    RUN_TEST(test_readOnly_allowed_in_response);
    RUN_TEST(test_writeOnly_rejected_in_response);
    RUN_TEST(test_writeOnly_allowed_in_request);
    RUN_TEST(test_readOnly_ignored_standalone);
    RUN_TEST(test_nested_readOnly_in_allOf);
    return UNITY_END();
}
