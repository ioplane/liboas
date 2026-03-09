#include <liboas/oas_schema.h>

#include <string.h>
#include <unity.h>

#include "parser/oas_json.h"
#include "parser/oas_schema_parser.h"

static oas_arena_t *arena;
static oas_error_list_t *errors;
static oas_json_doc_t jdoc;

void setUp(void)
{
    arena = oas_arena_create(0);
    errors = oas_error_list_create(arena);
    jdoc = (oas_json_doc_t){0};
}

void tearDown(void)
{
    oas_json_free(&jdoc);
    oas_arena_destroy(arena);
    arena = nullptr;
    errors = nullptr;
}

/* Helper: parse JSON string into schema (keeps yyjson doc alive until tearDown) */
static oas_schema_t *parse(const char *json)
{
    oas_json_free(&jdoc);
    int rc = oas_json_parse(json, strlen(json), &jdoc, errors);
    if (rc != 0) {
        return nullptr;
    }
    return oas_schema_parse(arena, jdoc.root, errors);
}

void test_parse_simple_string(void)
{
    oas_schema_t *s = parse("{\"type\": \"string\"}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, s->type_mask);
}

void test_parse_string_constraints(void)
{
    oas_schema_t *s = parse("{\"type\": \"string\", \"minLength\": 1, "
                            "\"maxLength\": 100, \"pattern\": \"^[a-z]+$\"}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT64(1, s->min_length);
    TEST_ASSERT_EQUAL_INT64(100, s->max_length);
    TEST_ASSERT_EQUAL_STRING("^[a-z]+$", s->pattern);
}

void test_parse_integer_range(void)
{
    oas_schema_t *s = parse("{\"type\": \"integer\", \"minimum\": 0, "
                            "\"maximum\": 100, \"exclusiveMinimum\": 0}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_INTEGER, s->type_mask);
    TEST_ASSERT_TRUE(s->has_minimum);
    TEST_ASSERT_TRUE(s->has_maximum);
    TEST_ASSERT_TRUE(s->has_exclusive_minimum);
    TEST_ASSERT_FALSE(s->has_exclusive_maximum);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, s->minimum);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 100.0, s->maximum);
}

void test_parse_array_items(void)
{
    oas_schema_t *s = parse("{\"type\": \"array\", \"items\": {\"type\": \"string\"}, "
                            "\"minItems\": 1, \"maxItems\": 10}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_ARRAY, s->type_mask);
    TEST_ASSERT_NOT_NULL(s->items);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, s->items->type_mask);
    TEST_ASSERT_EQUAL_INT64(1, s->min_items);
    TEST_ASSERT_EQUAL_INT64(10, s->max_items);
}

void test_parse_tuple_prefix_items(void)
{
    oas_schema_t *s = parse("{\"type\": \"array\", \"prefixItems\": ["
                            "{\"type\": \"string\"}, {\"type\": \"integer\"}]}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT64(2, s->prefix_items_count);
    TEST_ASSERT_NOT_NULL(s->prefix_items);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, s->prefix_items[0]->type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_INTEGER, s->prefix_items[1]->type_mask);
}

void test_parse_object_properties(void)
{
    oas_schema_t *s = parse("{\"type\": \"object\", \"properties\": {"
                            "\"name\": {\"type\": \"string\"}, "
                            "\"age\": {\"type\": \"integer\"}}, "
                            "\"required\": [\"name\"]}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_OBJECT, s->type_mask);
    TEST_ASSERT_EQUAL_UINT64(2, s->properties_count);
    TEST_ASSERT_EQUAL_UINT64(1, s->required_count);
    TEST_ASSERT_EQUAL_STRING("name", s->required[0]);
}

void test_parse_allof(void)
{
    oas_schema_t *s = parse("{\"allOf\": [{\"type\": \"object\"}, {\"type\": \"object\"}]}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT64(2, s->all_of_count);
    TEST_ASSERT_NOT_NULL(s->all_of);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_OBJECT, s->all_of[0]->type_mask);
}

void test_parse_oneof(void)
{
    oas_schema_t *s = parse("{\"oneOf\": [{\"type\": \"string\"}, {\"type\": \"integer\"}]}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT64(2, s->one_of_count);
}

void test_parse_anyof(void)
{
    oas_schema_t *s = parse("{\"anyOf\": [{\"type\": \"string\"}, {\"type\": \"null\"}]}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT64(2, s->any_of_count);
}

void test_parse_if_then_else(void)
{
    oas_schema_t *s = parse("{\"if\": {\"type\": \"string\"}, "
                            "\"then\": {\"type\": \"string\", \"minLength\": 1}, "
                            "\"else\": {\"type\": \"integer\"}}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(s->if_schema);
    TEST_ASSERT_NOT_NULL(s->then_schema);
    TEST_ASSERT_NOT_NULL(s->else_schema);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, s->if_schema->type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_INTEGER, s->else_schema->type_mask);
}

void test_parse_ref(void)
{
    oas_schema_t *s = parse("{\"$ref\": \"#/components/schemas/Pet\"}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Pet", s->ref);
    TEST_ASSERT_NULL(s->ref_resolved);
    /* $ref should not parse other keywords */
    TEST_ASSERT_EQUAL_UINT8(0, s->type_mask);
}

void test_parse_nullable(void)
{
    oas_schema_t *s = parse("{\"type\": \"string\", \"nullable\": true}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_TRUE(s->nullable);
    TEST_ASSERT_TRUE(s->type_mask & OAS_TYPE_STRING);
    TEST_ASSERT_TRUE(s->type_mask & OAS_TYPE_NULL);
}

/* ── $ref sibling keywords (OAS 3.1+ / JSON Schema 2020-12) ─────────── */

void test_schema_ref_with_description(void)
{
    oas_schema_t *s =
        parse("{\"$ref\": \"#/components/schemas/Pet\", \"description\": \"A pet override\"}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Pet", s->ref);
    TEST_ASSERT_EQUAL_STRING("A pet override", s->description);
}

void test_schema_ref_with_nullable(void)
{
    oas_schema_t *s = parse("{\"$ref\": \"#/components/schemas/Pet\", \"nullable\": true}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Pet", s->ref);
    TEST_ASSERT_TRUE(s->nullable);
}

void test_schema_ref_only(void)
{
    oas_schema_t *s = parse("{\"$ref\": \"#/components/schemas/Pet\"}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Pet", s->ref);
    TEST_ASSERT_EQUAL_UINT8(0, s->type_mask);
}

void test_schema_ref_with_type(void)
{
    oas_schema_t *s = parse("{\"$ref\": \"#/components/schemas/Base\", \"type\": \"object\"}");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Base", s->ref);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_OBJECT, s->type_mask);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_simple_string);
    RUN_TEST(test_parse_string_constraints);
    RUN_TEST(test_parse_integer_range);
    RUN_TEST(test_parse_array_items);
    RUN_TEST(test_parse_tuple_prefix_items);
    RUN_TEST(test_parse_object_properties);
    RUN_TEST(test_parse_allof);
    RUN_TEST(test_parse_oneof);
    RUN_TEST(test_parse_anyof);
    RUN_TEST(test_parse_if_then_else);
    RUN_TEST(test_parse_ref);
    RUN_TEST(test_parse_nullable);
    /* $ref siblings */
    RUN_TEST(test_schema_ref_with_description);
    RUN_TEST(test_schema_ref_with_nullable);
    RUN_TEST(test_schema_ref_only);
    RUN_TEST(test_schema_ref_with_type);
    return UNITY_END();
}
