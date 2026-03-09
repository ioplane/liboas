#include <liboas/oas_compiler.h>
#include <liboas/oas_parser.h>
#include <liboas/oas_regex.h>
#include <liboas/oas_schema.h>
#include <liboas/oas_validator.h>

#include "compiler/oas_format.h"
#include "compiler/oas_instruction.h"
#include "parser/oas_json.h"
#include "parser/oas_schema_parser.h"

#include <stdlib.h>
#include <string.h>

#include <yyjson.h>
#include <unity.h>

static oas_arena_t *arena;
static oas_regex_backend_t *regex;

void setUp(void)
{
    arena = oas_arena_create(0);
    regex = oas_regex_libregexp_create();
}

void tearDown(void)
{
    oas_arena_destroy(arena);
    arena = nullptr;
    if (regex) {
        regex->destroy(regex);
        regex = nullptr;
    }
}

static oas_validation_result_t validate_json(oas_compiled_schema_t *cs, const char *json)
{
    oas_validation_result_t result = {0};
    int rc = oas_validate_json(cs, json, strlen(json), &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    return result;
}

static oas_compiled_schema_t *compile_schema(oas_schema_t *s)
{
    oas_compiler_config_t config = {.regex = regex, .format_policy = OAS_FORMAT_IGNORE};
    oas_compiled_schema_t *cs = oas_schema_compile(s, &config, nullptr);
    TEST_ASSERT_NOT_NULL(cs);
    return cs;
}

/* ── minProperties / maxProperties ─────────────────────────────────────── */

void test_validate_min_properties_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;
    s->min_properties = 2;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "{\"a\":1, \"b\":2}");
    TEST_ASSERT_TRUE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_min_properties_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;
    s->min_properties = 2;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "{\"a\":1}");
    TEST_ASSERT_FALSE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_max_properties_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;
    s->max_properties = 2;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "{\"a\":1}");
    TEST_ASSERT_TRUE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_max_properties_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;
    s->max_properties = 2;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "{\"a\":1, \"b\":2, \"c\":3}");
    TEST_ASSERT_FALSE(r.valid);
    oas_compiled_schema_free(cs);
}

/* ── patternProperties ─────────────────────────────────────────────────── */

void test_validate_pattern_props_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *str_schema = oas_schema_create(arena);
    str_schema->type_mask = OAS_TYPE_STRING;

    oas_pattern_property_t *pp =
        oas_arena_alloc(arena, sizeof(*pp), _Alignof(oas_pattern_property_t));
    pp->pattern = "^x-";
    pp->schema = str_schema;
    s->pattern_properties = pp;
    s->pattern_properties_count = 1;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "{\"x-foo\": \"bar\"}");
    TEST_ASSERT_TRUE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_pattern_props_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *str_schema = oas_schema_create(arena);
    str_schema->type_mask = OAS_TYPE_STRING;

    oas_pattern_property_t *pp =
        oas_arena_alloc(arena, sizeof(*pp), _Alignof(oas_pattern_property_t));
    pp->pattern = "^x-";
    pp->schema = str_schema;
    s->pattern_properties = pp;
    s->pattern_properties_count = 1;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "{\"x-foo\": 42}");
    TEST_ASSERT_FALSE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_pattern_props_no_match(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *str_schema = oas_schema_create(arena);
    str_schema->type_mask = OAS_TYPE_STRING;

    oas_pattern_property_t *pp =
        oas_arena_alloc(arena, sizeof(*pp), _Alignof(oas_pattern_property_t));
    pp->pattern = "^x-";
    pp->schema = str_schema;
    s->pattern_properties = pp;
    s->pattern_properties_count = 1;

    oas_compiled_schema_t *cs = compile_schema(s);
    /* "name" doesn't match "^x-", so 42 is fine */
    oas_validation_result_t r = validate_json(cs, "{\"name\": 42}");
    TEST_ASSERT_TRUE(r.valid);
    oas_compiled_schema_free(cs);
}

/* ── propertyNames ─────────────────────────────────────────────────────── */

void test_validate_property_names_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *names_schema = oas_schema_create(arena);
    names_schema->type_mask = OAS_TYPE_STRING;
    names_schema->min_length = 2;
    s->property_names = names_schema;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "{\"ab\": 1, \"cd\": 2}");
    TEST_ASSERT_TRUE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_property_names_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *names_schema = oas_schema_create(arena);
    names_schema->type_mask = OAS_TYPE_STRING;
    names_schema->min_length = 2;
    s->property_names = names_schema;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "{\"a\": 1}");
    TEST_ASSERT_FALSE(r.valid);
    oas_compiled_schema_free(cs);
}

/* ── dependentRequired ─────────────────────────────────────────────────── */

void test_validate_dependent_required_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_dependent_required_t *dr =
        oas_arena_alloc(arena, sizeof(*dr), _Alignof(oas_dependent_required_t));
    dr->property = "a";
    const char **req = oas_arena_alloc(arena, sizeof(const char *), _Alignof(const char *));
    req[0] = "b";
    dr->required = req;
    dr->required_count = 1;
    s->dependent_required = dr;
    s->dependent_required_count = 1;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "{\"a\":1, \"b\":2}");
    TEST_ASSERT_TRUE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_dependent_required_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_dependent_required_t *dr =
        oas_arena_alloc(arena, sizeof(*dr), _Alignof(oas_dependent_required_t));
    dr->property = "a";
    const char **req = oas_arena_alloc(arena, sizeof(const char *), _Alignof(const char *));
    req[0] = "b";
    dr->required = req;
    dr->required_count = 1;
    s->dependent_required = dr;
    s->dependent_required_count = 1;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "{\"a\":1}");
    TEST_ASSERT_FALSE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_dependent_required_absent(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_dependent_required_t *dr =
        oas_arena_alloc(arena, sizeof(*dr), _Alignof(oas_dependent_required_t));
    dr->property = "a";
    const char **req = oas_arena_alloc(arena, sizeof(const char *), _Alignof(const char *));
    req[0] = "b";
    dr->required = req;
    dr->required_count = 1;
    s->dependent_required = dr;
    s->dependent_required_count = 1;

    oas_compiled_schema_t *cs = compile_schema(s);
    /* Trigger property "a" is absent, so no check needed */
    oas_validation_result_t r = validate_json(cs, "{\"c\":1}");
    TEST_ASSERT_TRUE(r.valid);
    oas_compiled_schema_free(cs);
}

/* ── dependentSchemas ──────────────────────────────────────────────────── */

void test_validate_dependent_schema_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    /* If "a" present, require "b" via dependent schema */
    oas_schema_t *dep_schema = oas_schema_create(arena);
    dep_schema->type_mask = OAS_TYPE_OBJECT;
    const char **req = oas_arena_alloc(arena, sizeof(const char *), _Alignof(const char *));
    req[0] = "b";
    dep_schema->required = req;
    dep_schema->required_count = 1;

    oas_dependent_schema_t *ds =
        oas_arena_alloc(arena, sizeof(*ds), _Alignof(oas_dependent_schema_t));
    ds->property = "a";
    ds->schema = dep_schema;
    s->dependent_schemas = ds;
    s->dependent_schemas_count = 1;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "{\"a\":1, \"b\":2}");
    TEST_ASSERT_TRUE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_dependent_schema_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *dep_schema = oas_schema_create(arena);
    dep_schema->type_mask = OAS_TYPE_OBJECT;
    const char **req = oas_arena_alloc(arena, sizeof(const char *), _Alignof(const char *));
    req[0] = "b";
    dep_schema->required = req;
    dep_schema->required_count = 1;

    oas_dependent_schema_t *ds =
        oas_arena_alloc(arena, sizeof(*ds), _Alignof(oas_dependent_schema_t));
    ds->property = "a";
    ds->schema = dep_schema;
    s->dependent_schemas = ds;
    s->dependent_schemas_count = 1;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "{\"a\":1}");
    TEST_ASSERT_FALSE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_dependent_schema_absent(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *dep_schema = oas_schema_create(arena);
    dep_schema->type_mask = OAS_TYPE_OBJECT;
    const char **req = oas_arena_alloc(arena, sizeof(const char *), _Alignof(const char *));
    req[0] = "b";
    dep_schema->required = req;
    dep_schema->required_count = 1;

    oas_dependent_schema_t *ds =
        oas_arena_alloc(arena, sizeof(*ds), _Alignof(oas_dependent_schema_t));
    ds->property = "a";
    ds->schema = dep_schema;
    s->dependent_schemas = ds;
    s->dependent_schemas_count = 1;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "{\"c\":1}");
    TEST_ASSERT_TRUE(r.valid);
    oas_compiled_schema_free(cs);
}

/* ── contains / minContains / maxContains ──────────────────────────────── */

void test_validate_contains_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_ARRAY;

    oas_schema_t *contains_schema = oas_schema_create(arena);
    contains_schema->type_mask = OAS_TYPE_STRING;
    s->contains = contains_schema;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "[1, \"a\", 2]");
    TEST_ASSERT_TRUE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_contains_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_ARRAY;

    oas_schema_t *contains_schema = oas_schema_create(arena);
    contains_schema->type_mask = OAS_TYPE_STRING;
    s->contains = contains_schema;

    oas_compiled_schema_t *cs = compile_schema(s);
    oas_validation_result_t r = validate_json(cs, "[1, 2, 3]");
    TEST_ASSERT_FALSE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_min_contains(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_ARRAY;

    oas_schema_t *contains_schema = oas_schema_create(arena);
    contains_schema->type_mask = OAS_TYPE_STRING;
    s->contains = contains_schema;
    s->min_contains = 2;

    oas_compiled_schema_t *cs = compile_schema(s);

    /* Only 1 string — needs at least 2 */
    oas_validation_result_t r = validate_json(cs, "[1, \"a\", 2]");
    TEST_ASSERT_FALSE(r.valid);

    oas_validation_result_t r2 = validate_json(cs, "[\"a\", \"b\", 2]");
    TEST_ASSERT_TRUE(r2.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_max_contains(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_ARRAY;

    oas_schema_t *contains_schema = oas_schema_create(arena);
    contains_schema->type_mask = OAS_TYPE_STRING;
    s->contains = contains_schema;
    s->min_contains = 1;
    s->max_contains = 1;

    oas_compiled_schema_t *cs = compile_schema(s);
    /* 2 strings — exceeds max of 1 */
    oas_validation_result_t r = validate_json(cs, "[\"a\", \"b\"]");
    TEST_ASSERT_FALSE(r.valid);

    oas_validation_result_t r2 = validate_json(cs, "[\"a\", 1]");
    TEST_ASSERT_TRUE(r2.valid);
    oas_compiled_schema_free(cs);
}

/* ── discriminator ─────────────────────────────────────────────────────── */

static oas_schema_t *make_discriminator_schema(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;
    s->discriminator_property = "petType";

    /* Branch 0: cat — requires "meow" property */
    oas_schema_t *cat = oas_schema_create(arena);
    cat->type_mask = OAS_TYPE_OBJECT;
    const char **cat_req = oas_arena_alloc(arena, sizeof(const char *), _Alignof(const char *));
    cat_req[0] = "meow";
    cat->required = cat_req;
    cat->required_count = 1;

    /* Branch 1: dog — requires "bark" property */
    oas_schema_t *dog = oas_schema_create(arena);
    dog->type_mask = OAS_TYPE_OBJECT;
    const char **dog_req = oas_arena_alloc(arena, sizeof(const char *), _Alignof(const char *));
    dog_req[0] = "bark";
    dog->required = dog_req;
    dog->required_count = 1;

    /* oneOf branches */
    oas_schema_t **branches =
        oas_arena_alloc(arena, 2 * sizeof(*branches), _Alignof(oas_schema_t *));
    branches[0] = cat;
    branches[1] = dog;
    s->one_of = branches;
    s->one_of_count = 2;

    /* Discriminator mapping: "cat" -> branch 0, "dog" -> branch 1 */
    oas_discriminator_mapping_t *mapping =
        oas_arena_alloc(arena, 2 * sizeof(*mapping), _Alignof(oas_discriminator_mapping_t));
    mapping[0].key = "cat";
    mapping[0].ref = "#/components/schemas/Cat";
    mapping[1].key = "dog";
    mapping[1].ref = "#/components/schemas/Dog";
    s->discriminator_mapping = mapping;
    s->discriminator_mapping_count = 2;

    return s;
}

void test_compile_discriminator_emits_opcode(void)
{
    oas_schema_t *s = make_discriminator_schema();
    oas_compiled_schema_t *cs = compile_schema(s);

    /* Verify the instruction stream contains OAS_OP_DISCRIMINATOR */
    const oas_program_t *prog = (const oas_program_t *)cs;
    bool found = false;
    for (size_t i = 0; i < prog->count; i++) {
        if (prog->code[i].op == OAS_OP_DISCRIMINATOR) {
            found = true;
            TEST_ASSERT_EQUAL_STRING("petType", prog->code[i].operand.str);
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "expected OAS_OP_DISCRIMINATOR in instruction stream");
    oas_compiled_schema_free(cs);
}

void test_validate_discriminator_match(void)
{
    oas_schema_t *s = make_discriminator_schema();
    oas_compiled_schema_t *cs = compile_schema(s);

    /* Cat with required "meow" property */
    oas_validation_result_t r = validate_json(cs, "{\"petType\": \"cat\", \"meow\": true}");
    TEST_ASSERT_TRUE(r.valid);

    /* Dog with required "bark" property */
    oas_validation_result_t r2 = validate_json(cs, "{\"petType\": \"dog\", \"bark\": true}");
    TEST_ASSERT_TRUE(r2.valid);

    oas_compiled_schema_free(cs);
}

void test_validate_discriminator_mismatch(void)
{
    oas_schema_t *s = make_discriminator_schema();
    oas_compiled_schema_t *cs = compile_schema(s);

    /* "fish" is not in the mapping */
    oas_validation_result_t r = validate_json(cs, "{\"petType\": \"fish\", \"swim\": true}");
    TEST_ASSERT_FALSE(r.valid);
    oas_compiled_schema_free(cs);
}

void test_validate_discriminator_missing_prop(void)
{
    oas_schema_t *s = make_discriminator_schema();
    oas_compiled_schema_t *cs = compile_schema(s);

    /* Missing discriminator property "petType" */
    oas_validation_result_t r = validate_json(cs, "{\"meow\": true}");
    TEST_ASSERT_FALSE(r.valid);
    oas_compiled_schema_free(cs);
}

/* ── propertyNames with pattern ────────────────────────────────────────── */

void test_validate_property_names_pattern(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    /* propertyNames must match ^[a-z]+$ */
    oas_schema_t *names_schema = oas_schema_create(arena);
    names_schema->type_mask = OAS_TYPE_STRING;
    names_schema->pattern = "^[a-z]+$";
    s->property_names = names_schema;

    oas_compiled_schema_t *cs = compile_schema(s);

    /* All lowercase — passes */
    oas_validation_result_t r = validate_json(cs, "{\"foo\": 1, \"bar\": 2}");
    TEST_ASSERT_TRUE(r.valid);

    /* "Baz" has uppercase — fails */
    oas_validation_result_t r2 = validate_json(cs, "{\"foo\": 1, \"Baz\": 2}");
    TEST_ASSERT_FALSE(r2.valid);

    oas_compiled_schema_free(cs);
}

/* ── contains without explicit minContains ─────────────────────────────── */

void test_validate_contains_without_min(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_ARRAY;

    oas_schema_t *contains_schema = oas_schema_create(arena);
    contains_schema->type_mask = OAS_TYPE_STRING;
    s->contains = contains_schema;
    /* min_contains left at -1 (not set), default should be 1 */

    oas_compiled_schema_t *cs = compile_schema(s);

    /* [1, "a"] has one string — passes with default minContains=1 */
    oas_validation_result_t r = validate_json(cs, "[1, \"a\"]");
    TEST_ASSERT_TRUE(r.valid);

    /* [1, 2] has no strings — fails */
    oas_validation_result_t r2 = validate_json(cs, "[1, 2]");
    TEST_ASSERT_FALSE(r2.valid);

    oas_compiled_schema_free(cs);
}

/* ── Parser: patternProperties ─────────────────────────────────────────── */

void test_parse_pattern_properties(void)
{
    const char *json = "{\"type\": \"object\", \"patternProperties\": "
                       "{\"^x-\": {\"type\": \"string\"}}}";
    oas_error_list_t *errors = oas_error_list_create(arena);
    oas_json_doc_t jdoc = {0};
    int rc = oas_json_parse(json, strlen(json), &jdoc, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_schema_t *s = oas_schema_parse(arena, jdoc.root, errors);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT64(1, s->pattern_properties_count);
    TEST_ASSERT_EQUAL_STRING("^x-", s->pattern_properties[0].pattern);
    TEST_ASSERT_NOT_NULL(s->pattern_properties[0].schema);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, s->pattern_properties[0].schema->type_mask);

    oas_json_free(&jdoc);
}

/* ── Parser: dependentRequired ─────────────────────────────────────────── */

void test_parse_dependent_required(void)
{
    const char *json = "{\"type\": \"object\", \"dependentRequired\": "
                       "{\"a\": [\"b\", \"c\"]}}";
    oas_error_list_t *errors = oas_error_list_create(arena);
    oas_json_doc_t jdoc = {0};
    int rc = oas_json_parse(json, strlen(json), &jdoc, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_schema_t *s = oas_schema_parse(arena, jdoc.root, errors);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT64(1, s->dependent_required_count);
    TEST_ASSERT_EQUAL_STRING("a", s->dependent_required[0].property);
    TEST_ASSERT_EQUAL_UINT64(2, s->dependent_required[0].required_count);
    TEST_ASSERT_EQUAL_STRING("b", s->dependent_required[0].required[0]);
    TEST_ASSERT_EQUAL_STRING("c", s->dependent_required[0].required[1]);

    oas_json_free(&jdoc);
}

/* ── Parser: contains ──────────────────────────────────────────────────── */

void test_parse_contains(void)
{
    const char *json = "{\"type\": \"array\", \"contains\": {\"type\": \"string\"}, "
                       "\"minContains\": 2, \"maxContains\": 5}";
    oas_error_list_t *errors = oas_error_list_create(arena);
    oas_json_doc_t jdoc = {0};
    int rc = oas_json_parse(json, strlen(json), &jdoc, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_schema_t *s = oas_schema_parse(arena, jdoc.root, errors);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(s->contains);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, s->contains->type_mask);
    TEST_ASSERT_EQUAL_INT64(2, s->min_contains);
    TEST_ASSERT_EQUAL_INT64(5, s->max_contains);

    oas_json_free(&jdoc);
}

/* ── Parser: Schema.deprecated ─────────────────────────────────────────── */

void test_parse_schema_deprecated(void)
{
    const char *json = "{\"type\": \"string\", \"deprecated\": true}";
    oas_error_list_t *errors = oas_error_list_create(arena);
    oas_json_doc_t jdoc = {0};
    int rc = oas_json_parse(json, strlen(json), &jdoc, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_schema_t *s = oas_schema_parse(arena, jdoc.root, errors);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_TRUE(s->deprecated);

    oas_json_free(&jdoc);
}

/* ── Parser: Info.summary ──────────────────────────────────────────────── */

void test_parse_info_summary(void)
{
    const char *json = "{\"openapi\": \"3.1.0\", \"info\": {\"title\": \"Test\", "
                       "\"summary\": \"A brief description\", \"version\": \"1.0\"}, "
                       "\"paths\": {}}";
    oas_error_list_t *errors = oas_error_list_create(arena);
    oas_doc_t *doc = oas_doc_parse(arena, json, strlen(json), errors);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_NOT_NULL(doc->info);
    TEST_ASSERT_EQUAL_STRING("A brief description", doc->info->summary);
    oas_doc_free(doc);
}

int main(void)
{
    UNITY_BEGIN();
    /* minProperties / maxProperties */
    RUN_TEST(test_validate_min_properties_pass);
    RUN_TEST(test_validate_min_properties_fail);
    RUN_TEST(test_validate_max_properties_pass);
    RUN_TEST(test_validate_max_properties_fail);
    /* patternProperties */
    RUN_TEST(test_validate_pattern_props_pass);
    RUN_TEST(test_validate_pattern_props_fail);
    RUN_TEST(test_validate_pattern_props_no_match);
    /* propertyNames */
    RUN_TEST(test_validate_property_names_pass);
    RUN_TEST(test_validate_property_names_fail);
    /* dependentRequired */
    RUN_TEST(test_validate_dependent_required_pass);
    RUN_TEST(test_validate_dependent_required_fail);
    RUN_TEST(test_validate_dependent_required_absent);
    /* dependentSchemas */
    RUN_TEST(test_validate_dependent_schema_pass);
    RUN_TEST(test_validate_dependent_schema_fail);
    RUN_TEST(test_validate_dependent_schema_absent);
    /* contains */
    RUN_TEST(test_validate_contains_pass);
    RUN_TEST(test_validate_contains_fail);
    RUN_TEST(test_validate_min_contains);
    RUN_TEST(test_validate_max_contains);
    /* discriminator */
    RUN_TEST(test_compile_discriminator_emits_opcode);
    RUN_TEST(test_validate_discriminator_match);
    RUN_TEST(test_validate_discriminator_mismatch);
    RUN_TEST(test_validate_discriminator_missing_prop);
    /* propertyNames with pattern */
    RUN_TEST(test_validate_property_names_pattern);
    /* contains without explicit minContains */
    RUN_TEST(test_validate_contains_without_min);
    /* Parser tests */
    RUN_TEST(test_parse_pattern_properties);
    RUN_TEST(test_parse_dependent_required);
    RUN_TEST(test_parse_contains);
    RUN_TEST(test_parse_schema_deprecated);
    RUN_TEST(test_parse_info_summary);
    return UNITY_END();
}
