#include <liboas/oas_schema.h>

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

void test_schema_type_mask_single(void)
{
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, oas_type_from_string("string"));
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_INTEGER, oas_type_from_string("integer"));
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_NUMBER, oas_type_from_string("number"));
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_BOOLEAN, oas_type_from_string("boolean"));
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_ARRAY, oas_type_from_string("array"));
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_OBJECT, oas_type_from_string("object"));
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_NULL, oas_type_from_string("null"));
    TEST_ASSERT_EQUAL_UINT8(0, oas_type_from_string("unknown"));
    TEST_ASSERT_EQUAL_UINT8(0, oas_type_from_string(nullptr));
}

void test_schema_type_mask_array(void)
{
    /* ["string", "null"] -> bitmask with both set */
    uint8_t mask = oas_type_from_string("string") | oas_type_from_string("null");
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING | OAS_TYPE_NULL, mask);
    TEST_ASSERT_TRUE(mask & OAS_TYPE_STRING);
    TEST_ASSERT_TRUE(mask & OAS_TYPE_NULL);
    TEST_ASSERT_FALSE(mask & OAS_TYPE_INTEGER);
}

void test_schema_string_constraints(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(s);

    /* Defaults */
    TEST_ASSERT_EQUAL_INT64(-1, s->min_length);
    TEST_ASSERT_EQUAL_INT64(-1, s->max_length);
    TEST_ASSERT_NULL(s->pattern);

    s->type_mask = OAS_TYPE_STRING;
    s->min_length = 1;
    s->max_length = 255;
    s->pattern = "^[a-z]+$";

    TEST_ASSERT_EQUAL_INT64(1, s->min_length);
    TEST_ASSERT_EQUAL_INT64(255, s->max_length);
    TEST_ASSERT_EQUAL_STRING("^[a-z]+$", s->pattern);
}

void test_schema_numeric_constraints(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(s);

    s->type_mask = OAS_TYPE_INTEGER;
    s->minimum = 0.0;
    s->maximum = 100.0;
    s->has_minimum = true;
    s->has_maximum = true;
    s->exclusive_minimum = 0.0;
    s->has_exclusive_minimum = true;
    s->multiple_of = 5.0;
    s->has_multiple_of = true;

    TEST_ASSERT_TRUE(s->has_minimum);
    TEST_ASSERT_TRUE(s->has_maximum);
    TEST_ASSERT_TRUE(s->has_exclusive_minimum);
    TEST_ASSERT_FALSE(s->has_exclusive_maximum);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 100.0, s->maximum);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 5.0, s->multiple_of);
}

void test_schema_array_items(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *items = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(items);

    s->type_mask = OAS_TYPE_ARRAY;
    items->type_mask = OAS_TYPE_STRING;
    s->items = items;
    s->min_items = 1;
    s->max_items = 10;

    TEST_ASSERT_EQUAL_PTR(items, s->items);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, s->items->type_mask);
    TEST_ASSERT_EQUAL_INT64(1, s->min_items);
    TEST_ASSERT_EQUAL_INT64(10, s->max_items);
}

void test_schema_object_properties(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *name_s = oas_schema_create(arena);
    oas_schema_t *age_s = oas_schema_create(arena);

    s->type_mask = OAS_TYPE_OBJECT;
    name_s->type_mask = OAS_TYPE_STRING;
    age_s->type_mask = OAS_TYPE_INTEGER;

    int rc = oas_schema_add_property(arena, s, "name", name_s);
    TEST_ASSERT_EQUAL_INT(0, rc);
    rc = oas_schema_add_property(arena, s, "age", age_s);
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_EQUAL_UINT64(2, s->properties_count);
    TEST_ASSERT_NOT_NULL(s->properties);
}

void test_schema_required_fields(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(s);

    const char **req = oas_arena_alloc(arena, sizeof(const char *) * 2, _Alignof(const char *));
    TEST_ASSERT_NOT_NULL(req);
    req[0] = "name";
    req[1] = "email";
    s->required = req;
    s->required_count = 2;

    TEST_ASSERT_EQUAL_UINT64(2, s->required_count);
    TEST_ASSERT_EQUAL_STRING("name", s->required[0]);
    TEST_ASSERT_EQUAL_STRING("email", s->required[1]);
}

void test_schema_composition_allof(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *s1 = oas_schema_create(arena);
    oas_schema_t *s2 = oas_schema_create(arena);

    s1->type_mask = OAS_TYPE_OBJECT;
    s2->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t **all_of =
        oas_arena_alloc(arena, sizeof(oas_schema_t *) * 2, _Alignof(oas_schema_t *));
    TEST_ASSERT_NOT_NULL(all_of);
    all_of[0] = s1;
    all_of[1] = s2;
    s->all_of = all_of;
    s->all_of_count = 2;

    TEST_ASSERT_EQUAL_UINT64(2, s->all_of_count);
    TEST_ASSERT_EQUAL_PTR(s1, s->all_of[0]);
    TEST_ASSERT_EQUAL_PTR(s2, s->all_of[1]);
}

void test_schema_ref_string(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(s);

    s->ref = "#/components/schemas/Pet";
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Pet", s->ref);
    TEST_ASSERT_NULL(s->ref_resolved);
}

void test_schema_nullable_to_type_mask(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(s);

    s->type_mask = OAS_TYPE_STRING;
    s->nullable = true;

    /* Apply nullable -> add OAS_TYPE_NULL to mask */
    if (s->nullable) {
        s->type_mask |= OAS_TYPE_NULL;
    }

    TEST_ASSERT_TRUE(s->type_mask & OAS_TYPE_STRING);
    TEST_ASSERT_TRUE(s->type_mask & OAS_TYPE_NULL);
}

void test_schema_discriminator(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(s);

    s->discriminator_property = "petType";

    oas_discriminator_mapping_t *mappings = oas_arena_alloc(
        arena, sizeof(oas_discriminator_mapping_t) * 2, _Alignof(oas_discriminator_mapping_t));
    TEST_ASSERT_NOT_NULL(mappings);
    mappings[0].key = "cat";
    mappings[0].ref = "#/components/schemas/Cat";
    mappings[1].key = "dog";
    mappings[1].ref = "#/components/schemas/Dog";
    s->discriminator_mapping = mappings;
    s->discriminator_mapping_count = 2;

    TEST_ASSERT_EQUAL_STRING("petType", s->discriminator_property);
    TEST_ASSERT_EQUAL_UINT64(2, s->discriminator_mapping_count);
    TEST_ASSERT_EQUAL_STRING("cat", s->discriminator_mapping[0].key);
    TEST_ASSERT_EQUAL_STRING("#/components/schemas/Dog", s->discriminator_mapping[1].ref);
}

void test_schema_unevaluated_properties(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(s);

    TEST_ASSERT_FALSE(s->has_unevaluated_properties);

    oas_schema_t *uneval = oas_schema_create(arena);
    s->has_unevaluated_properties = true;
    s->unevaluated_properties = uneval;

    TEST_ASSERT_TRUE(s->has_unevaluated_properties);
    TEST_ASSERT_NOT_NULL(s->unevaluated_properties);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_schema_type_mask_single);
    RUN_TEST(test_schema_type_mask_array);
    RUN_TEST(test_schema_string_constraints);
    RUN_TEST(test_schema_numeric_constraints);
    RUN_TEST(test_schema_array_items);
    RUN_TEST(test_schema_object_properties);
    RUN_TEST(test_schema_required_fields);
    RUN_TEST(test_schema_composition_allof);
    RUN_TEST(test_schema_ref_string);
    RUN_TEST(test_schema_nullable_to_type_mask);
    RUN_TEST(test_schema_discriminator);
    RUN_TEST(test_schema_unevaluated_properties);
    return UNITY_END();
}
