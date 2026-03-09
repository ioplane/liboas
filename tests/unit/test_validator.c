#include <liboas/oas_compiler.h>
#include <liboas/oas_regex.h>
#include <liboas/oas_schema.h>
#include <liboas/oas_validator.h>

#include "compiler/oas_format.h"
#include "compiler/oas_instruction.h"

#include <string.h>

#include <unity.h>
#include <yyjson.h>

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

/* Helper: compile schema and validate JSON string */
static oas_validation_result_t validate_json(oas_compiled_schema_t *cs, const char *json)
{
    oas_validation_result_t result = {0};
    int rc = oas_validate_json(cs, json, strlen(json), &result, arena);
    TEST_ASSERT_EQUAL_INT(0, rc);
    return result;
}

/* Helper: compile a schema with the regex backend */
static oas_compiled_schema_t *compile_schema(oas_schema_t *s)
{
    oas_compiler_config_t config = {.regex = regex, .format_policy = OAS_FORMAT_IGNORE};
    oas_compiled_schema_t *cs = oas_schema_compile(s, &config, nullptr);
    TEST_ASSERT_NOT_NULL(cs);
    return cs;
}

/* ── Test 1: type string pass ─────────────────────────────────────────── */

void test_validator_type_string_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "\"hello\"");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 2: type string fail ─────────────────────────────────────────── */

void test_validator_type_string_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "42");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 3: type integer pass ────────────────────────────────────────── */

void test_validator_type_integer_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_INTEGER;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "42");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 4: type integer float ───────────────────────────────────────── */

void test_validator_type_integer_float(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_INTEGER;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "3.14");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 5: type number ──────────────────────────────────────────────── */

void test_validator_type_number(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_NUMBER;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "3.14");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 6: type null ────────────────────────────────────────────────── */

void test_validator_type_null(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_NULL;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "null");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 7: type multi ───────────────────────────────────────────────── */

void test_validator_type_multi(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING | OAS_TYPE_INTEGER;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r1 = validate_json(cs, "\"hello\"");
    TEST_ASSERT_TRUE(r1.valid);

    oas_validation_result_t r2 = validate_json(cs, "42");
    TEST_ASSERT_TRUE(r2.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 8: min length pass ──────────────────────────────────────────── */

void test_validator_min_length_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->min_length = 3;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "\"abc\"");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 9: min length unicode ───────────────────────────────────────── */

void test_validator_min_length_unicode(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->min_length = 2;
    oas_compiled_schema_t *cs = compile_schema(s);

    /* "über" is 4 codepoints (ü is 2 bytes in UTF-8 but 1 codepoint) */
    oas_validation_result_t r = validate_json(cs, "\"\\u00fcber\"");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 10: max length fail ─────────────────────────────────────────── */

void test_validator_max_length_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->max_length = 3;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "\"abcd\"");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 11: pattern pass ────────────────────────────────────────────── */

void test_validator_pattern_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->pattern = "^[a-z]+$";
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "\"hello\"");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 12: pattern fail ────────────────────────────────────────────── */

void test_validator_pattern_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->pattern = "^[a-z]+$";
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "\"Hello\"");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 13: minimum pass ────────────────────────────────────────────── */

void test_validator_minimum_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->has_minimum = true;
    s->minimum = 0.0;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "5");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 14: exclusive max fail ──────────────────────────────────────── */

void test_validator_exclusive_max_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->has_exclusive_maximum = true;
    s->exclusive_maximum = 10.0;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "10");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 15: multiple of ─────────────────────────────────────────────── */

void test_validator_multiple_of(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->has_multiple_of = true;
    s->multiple_of = 3.0;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r1 = validate_json(cs, "9");
    TEST_ASSERT_TRUE(r1.valid);

    oas_validation_result_t r2 = validate_json(cs, "10");
    TEST_ASSERT_FALSE(r2.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 16: required pass ───────────────────────────────────────────── */

void test_validator_required_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    const char *req[] = {"name"};
    s->required = req;
    s->required_count = 1;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "{\"name\":\"x\"}");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 17: required fail ───────────────────────────────────────────── */

void test_validator_required_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    const char *req[] = {"name"};
    s->required = req;
    s->required_count = 1;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "{\"age\":5}");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 18: properties nested ───────────────────────────────────────── */

void test_validator_properties_nested(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;
    oas_schema_t *name_s = oas_schema_create(arena);
    name_s->type_mask = OAS_TYPE_STRING;
    int rc = oas_schema_add_property(arena, s, "name", name_s);
    TEST_ASSERT_EQUAL_INT(0, rc);
    oas_compiled_schema_t *cs = compile_schema(s);

    /* Valid: name is a string */
    oas_validation_result_t r1 = validate_json(cs, "{\"name\":\"Alice\"}");
    TEST_ASSERT_TRUE(r1.valid);

    /* Invalid: name is a number */
    oas_validation_result_t r2 = validate_json(cs, "{\"name\":42}");
    TEST_ASSERT_FALSE(r2.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 19: array items ─────────────────────────────────────────────── */

void test_validator_array_items(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *item = oas_schema_create(arena);
    item->type_mask = OAS_TYPE_STRING;
    s->items = item;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r1 = validate_json(cs, "[\"a\",\"b\"]");
    TEST_ASSERT_TRUE(r1.valid);

    oas_validation_result_t r2 = validate_json(cs, "[\"a\",1]");
    TEST_ASSERT_FALSE(r2.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 20: array min items fail ────────────────────────────────────── */

void test_validator_array_min_items_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->min_items = 3;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "[1,2]");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 21: enum pass ───────────────────────────────────────────────── */

void test_validator_enum_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_str(doc, arr, "a");
    yyjson_mut_arr_add_str(doc, arr, "b");
    yyjson_mut_doc_set_root(doc, arr);
    yyjson_doc *idoc = yyjson_mut_doc_imut_copy(doc, nullptr);
    s->enum_values = yyjson_doc_get_root(idoc);
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "\"a\"");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
    yyjson_doc_free(idoc);
    yyjson_mut_doc_free(doc);
}

/* ── Test 22: enum fail ───────────────────────────────────────────────── */

void test_validator_enum_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_str(doc, arr, "a");
    yyjson_mut_arr_add_str(doc, arr, "b");
    yyjson_mut_doc_set_root(doc, arr);
    yyjson_doc *idoc = yyjson_mut_doc_imut_copy(doc, nullptr);
    s->enum_values = yyjson_doc_get_root(idoc);
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "\"c\"");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
    yyjson_doc_free(idoc);
    yyjson_mut_doc_free(doc);
}

/* ── Test 23: const ───────────────────────────────────────────────────── */

void test_validator_const(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *val = yyjson_mut_str(doc, "fixed");
    yyjson_mut_doc_set_root(doc, val);
    yyjson_doc *idoc = yyjson_mut_doc_imut_copy(doc, nullptr);
    s->const_value = yyjson_doc_get_root(idoc);
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r1 = validate_json(cs, "\"fixed\"");
    TEST_ASSERT_TRUE(r1.valid);

    oas_validation_result_t r2 = validate_json(cs, "\"other\"");
    TEST_ASSERT_FALSE(r2.valid);

    oas_compiled_schema_free(cs);
    yyjson_doc_free(idoc);
    yyjson_mut_doc_free(doc);
}

/* ── Test 24: allOf pass ──────────────────────────────────────────────── */

void test_validator_allof_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *s1 = oas_schema_create(arena);
    s1->type_mask = OAS_TYPE_STRING;
    oas_schema_t *s2 = oas_schema_create(arena);
    s2->min_length = 3;

    oas_schema_t *branches[2] = {s1, s2};
    s->all_of = branches;
    s->all_of_count = 2;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "\"hello\"");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 25: allOf fail ──────────────────────────────────────────────── */

void test_validator_allof_fail(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *s1 = oas_schema_create(arena);
    s1->type_mask = OAS_TYPE_STRING;
    oas_schema_t *s2 = oas_schema_create(arena);
    s2->min_length = 10;

    oas_schema_t *branches[2] = {s1, s2};
    s->all_of = branches;
    s->all_of_count = 2;
    oas_compiled_schema_t *cs = compile_schema(s);

    /* "hi" passes type check but fails minLength:10 */
    oas_validation_result_t r = validate_json(cs, "\"hi\"");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 26: oneOf pass ──────────────────────────────────────────────── */

void test_validator_oneof_pass(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *s1 = oas_schema_create(arena);
    s1->type_mask = OAS_TYPE_STRING;
    oas_schema_t *s2 = oas_schema_create(arena);
    s2->type_mask = OAS_TYPE_INTEGER;

    oas_schema_t *branches[2] = {s1, s2};
    s->one_of = branches;
    s->one_of_count = 2;
    oas_compiled_schema_t *cs = compile_schema(s);

    oas_validation_result_t r = validate_json(cs, "\"hello\"");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 27: oneOf fail zero ─────────────────────────────────────────── */

void test_validator_oneof_fail_zero(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *s1 = oas_schema_create(arena);
    s1->type_mask = OAS_TYPE_STRING;
    oas_schema_t *s2 = oas_schema_create(arena);
    s2->type_mask = OAS_TYPE_INTEGER;

    oas_schema_t *branches[2] = {s1, s2};
    s->one_of = branches;
    s->one_of_count = 2;
    oas_compiled_schema_t *cs = compile_schema(s);

    /* null matches neither */
    oas_validation_result_t r = validate_json(cs, "null");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 28: oneOf fail two ──────────────────────────────────────────── */

void test_validator_oneof_fail_two(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    /* Both branches accept numbers */
    oas_schema_t *s1 = oas_schema_create(arena);
    s1->type_mask = OAS_TYPE_NUMBER;
    oas_schema_t *s2 = oas_schema_create(arena);
    s2->type_mask = OAS_TYPE_INTEGER;

    oas_schema_t *branches[2] = {s1, s2};
    s->one_of = branches;
    s->one_of_count = 2;
    oas_compiled_schema_t *cs = compile_schema(s);

    /* 42 is both number and integer — matches both branches */
    oas_validation_result_t r = validate_json(cs, "42");
    TEST_ASSERT_FALSE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 29: not ─────────────────────────────────────────────────────── */

void test_validator_not(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *inner = oas_schema_create(arena);
    inner->type_mask = OAS_TYPE_STRING;
    s->not_schema = inner;
    oas_compiled_schema_t *cs = compile_schema(s);

    /* 42 is not a string — passes "not" */
    oas_validation_result_t r1 = validate_json(cs, "42");
    TEST_ASSERT_TRUE(r1.valid);

    /* "hi" is a string — fails "not" */
    oas_validation_result_t r2 = validate_json(cs, "\"hi\"");
    TEST_ASSERT_FALSE(r2.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 30: if/then/else ────────────────────────────────────────────── */

void test_validator_if_then_else(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *if_s = oas_schema_create(arena);
    if_s->type_mask = OAS_TYPE_STRING;
    oas_schema_t *then_s = oas_schema_create(arena);
    then_s->min_length = 1;
    oas_schema_t *else_s = oas_schema_create(arena);
    else_s->type_mask = OAS_TYPE_INTEGER;

    s->if_schema = if_s;
    s->then_schema = then_s;
    s->else_schema = else_s;
    oas_compiled_schema_t *cs = compile_schema(s);

    /* String "hello" — if passes, then minLength:1 passes */
    oas_validation_result_t r1 = validate_json(cs, "\"hello\"");
    TEST_ASSERT_TRUE(r1.valid);

    /* Integer 42 — if fails, else type:integer passes */
    oas_validation_result_t r2 = validate_json(cs, "42");
    TEST_ASSERT_TRUE(r2.valid);

    /* Boolean — if fails, else type:integer fails */
    oas_validation_result_t r3 = validate_json(cs, "true");
    TEST_ASSERT_FALSE(r3.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 31: additional properties ───────────────────────────────────── */

void test_validator_additional_properties(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;
    oas_schema_t *addl = oas_schema_create(arena);
    addl->type_mask = OAS_TYPE_STRING;
    s->additional_properties = addl;
    oas_compiled_schema_t *cs = compile_schema(s);

    /* Should not crash — additional properties validation is skipped for now */
    oas_validation_result_t r = validate_json(cs, "{\"x\":\"hello\"}");
    TEST_ASSERT_TRUE(r.valid);

    oas_compiled_schema_free(cs);
}

/* ── Test 32: collect all errors ──────────────────────────────────────── */

void test_validator_collect_all_errors(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING;
    s->min_length = 10;
    const char *req[] = {"name"};
    s->required = req;
    s->required_count = 1;
    oas_compiled_schema_t *cs = compile_schema(s);

    /* 42 fails type check; minLength skipped (not string); required skipped (not object) */
    oas_validation_result_t r = validate_json(cs, "42");
    TEST_ASSERT_FALSE(r.valid);
    TEST_ASSERT_NOT_NULL(r.errors);
    TEST_ASSERT_GREATER_OR_EQUAL(1, (int)oas_error_list_count(r.errors));

    oas_compiled_schema_free(cs);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_validator_type_string_pass);
    RUN_TEST(test_validator_type_string_fail);
    RUN_TEST(test_validator_type_integer_pass);
    RUN_TEST(test_validator_type_integer_float);
    RUN_TEST(test_validator_type_number);
    RUN_TEST(test_validator_type_null);
    RUN_TEST(test_validator_type_multi);
    RUN_TEST(test_validator_min_length_pass);
    RUN_TEST(test_validator_min_length_unicode);
    RUN_TEST(test_validator_max_length_fail);
    RUN_TEST(test_validator_pattern_pass);
    RUN_TEST(test_validator_pattern_fail);
    RUN_TEST(test_validator_minimum_pass);
    RUN_TEST(test_validator_exclusive_max_fail);
    RUN_TEST(test_validator_multiple_of);
    RUN_TEST(test_validator_required_pass);
    RUN_TEST(test_validator_required_fail);
    RUN_TEST(test_validator_properties_nested);
    RUN_TEST(test_validator_array_items);
    RUN_TEST(test_validator_array_min_items_fail);
    RUN_TEST(test_validator_enum_pass);
    RUN_TEST(test_validator_enum_fail);
    RUN_TEST(test_validator_const);
    RUN_TEST(test_validator_allof_pass);
    RUN_TEST(test_validator_allof_fail);
    RUN_TEST(test_validator_oneof_pass);
    RUN_TEST(test_validator_oneof_fail_zero);
    RUN_TEST(test_validator_oneof_fail_two);
    RUN_TEST(test_validator_not);
    RUN_TEST(test_validator_if_then_else);
    RUN_TEST(test_validator_additional_properties);
    RUN_TEST(test_validator_collect_all_errors);
    return UNITY_END();
}
