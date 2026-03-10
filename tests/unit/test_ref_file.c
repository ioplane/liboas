#include "parser/oas_ref.h"

#include "parser/oas_ref_cache.h"

#include <liboas/oas_alloc.h>
#include <liboas/oas_error.h>

#include <errno.h>
#include <string.h>
#include <unity.h>

#include <yyjson.h>

static oas_ref_cache_t *cache;
static oas_arena_t *arena;
static oas_error_list_t *errors;

void setUp(void)
{
    cache = oas_ref_cache_create(0);
    arena = oas_arena_create(0);
    errors = oas_error_list_create(arena);
}

void tearDown(void)
{
    oas_ref_cache_destroy(cache);
    cache = nullptr;
    oas_arena_destroy(arena);
    arena = nullptr;
    errors = nullptr;
}

void test_file_load_json(void)
{
    yyjson_val *root = nullptr;
    int rc = oas_ref_load_file("tests/fixtures/ref_pet.json", nullptr, cache, 0, &root, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(root);

    /* Verify root is an object with "Pet" key */
    TEST_ASSERT_TRUE(yyjson_is_obj(root));
    yyjson_val *pet = yyjson_obj_get(root, "Pet");
    TEST_ASSERT_NOT_NULL(pet);
    TEST_ASSERT_TRUE(yyjson_is_obj(pet));

    /* Verify nested structure */
    yyjson_val *type_val = yyjson_obj_get(pet, "type");
    TEST_ASSERT_NOT_NULL(type_val);
    TEST_ASSERT_EQUAL_STRING("object", yyjson_get_str(type_val));
}

void test_file_load_relative_path(void)
{
    yyjson_val *root = nullptr;

    /* Load with explicit base_dir + relative path */
    int rc = oas_ref_load_file("ref_pet.json", "tests/fixtures", cache, 0, &root, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(root);

    /* Verify content is the same fixture */
    TEST_ASSERT_TRUE(yyjson_is_obj(root));
    yyjson_val *pet = yyjson_obj_get(root, "Pet");
    TEST_ASSERT_NOT_NULL(pet);
}

void test_file_not_found(void)
{
    yyjson_val *root = nullptr;
    int rc = oas_ref_load_file("tests/fixtures/nonexistent.json", nullptr, cache, 0, &root, errors);
    TEST_ASSERT_EQUAL_INT(-ENOENT, rc);
    TEST_ASSERT_NULL(root);
}

void test_file_path_traversal_blocked(void)
{
    yyjson_val *root = nullptr;
    int rc = oas_ref_load_file("../../etc/passwd", "tests/fixtures", cache, 0, &root, errors);
    TEST_ASSERT_EQUAL_INT(-EINVAL, rc);
    TEST_ASSERT_NULL(root);
}

void test_file_cached_on_second_load(void)
{
    yyjson_val *root1 = nullptr;
    int rc = oas_ref_load_file("tests/fixtures/ref_pet.json", nullptr, cache, 0, &root1, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(root1);
    TEST_ASSERT_EQUAL_size_t(1, oas_ref_cache_count(cache));

    /* Second load should hit cache */
    yyjson_val *root2 = nullptr;
    rc = oas_ref_load_file("tests/fixtures/ref_pet.json", nullptr, cache, 0, &root2, errors);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(root2);

    /* Same pointer returned from cache */
    TEST_ASSERT_EQUAL_PTR(root1, root2);

    /* Count unchanged — file was not re-parsed */
    TEST_ASSERT_EQUAL_size_t(1, oas_ref_cache_count(cache));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_file_load_json);
    RUN_TEST(test_file_load_relative_path);
    RUN_TEST(test_file_not_found);
    RUN_TEST(test_file_path_traversal_blocked);
    RUN_TEST(test_file_cached_on_second_load);
    return UNITY_END();
}
