#include "parser/oas_ref_cache.h"

#include <errno.h>
#include <string.h>
#include <unity.h>

void setUp(void)
{
}

void tearDown(void)
{
}

/* Helper: parse a minimal JSON document */
static yyjson_doc *make_doc(const char *json)
{
    return yyjson_read(json, strlen(json), 0);
}

void test_cache_create_destroy(void)
{
    /* Default max_documents */
    oas_ref_cache_t *cache = oas_ref_cache_create(0);
    TEST_ASSERT_NOT_NULL(cache);
    TEST_ASSERT_EQUAL_size_t(0, oas_ref_cache_count(cache));
    oas_ref_cache_destroy(cache);

    /* Explicit max */
    cache = oas_ref_cache_create(50);
    TEST_ASSERT_NOT_NULL(cache);
    TEST_ASSERT_EQUAL_size_t(0, oas_ref_cache_count(cache));
    oas_ref_cache_destroy(cache);
}

void test_cache_put_get(void)
{
    oas_ref_cache_t *cache = oas_ref_cache_create(10);
    TEST_ASSERT_NOT_NULL(cache);

    yyjson_doc *doc = make_doc("{\"type\": \"object\"}");
    TEST_ASSERT_NOT_NULL(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    int rc = oas_ref_cache_put(cache, "https://example.com/schema.json", doc, root);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_size_t(1, oas_ref_cache_count(cache));

    yyjson_val *got = oas_ref_cache_get(cache, "https://example.com/schema.json");
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_PTR(root, got);

    /* Cache owns doc, destroy frees it */
    oas_ref_cache_destroy(cache);
}

void test_cache_miss_returns_null(void)
{
    oas_ref_cache_t *cache = oas_ref_cache_create(10);
    TEST_ASSERT_NOT_NULL(cache);

    yyjson_val *got = oas_ref_cache_get(cache, "https://example.com/missing.json");
    TEST_ASSERT_NULL(got);

    /* Also test nullptr args */
    TEST_ASSERT_NULL(oas_ref_cache_get(cache, nullptr));
    TEST_ASSERT_NULL(oas_ref_cache_get(nullptr, "uri"));

    oas_ref_cache_destroy(cache);
}

void test_cache_dedup(void)
{
    oas_ref_cache_t *cache = oas_ref_cache_create(10);
    TEST_ASSERT_NOT_NULL(cache);

    yyjson_doc *doc1 = make_doc("{\"id\": 1}");
    TEST_ASSERT_NOT_NULL(doc1);
    yyjson_val *root1 = yyjson_doc_get_root(doc1);

    int rc = oas_ref_cache_put(cache, "https://example.com/a.json", doc1, root1);
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* Second put with same URI is a no-op */
    yyjson_doc *doc2 = make_doc("{\"id\": 2}");
    TEST_ASSERT_NOT_NULL(doc2);
    yyjson_val *root2 = yyjson_doc_get_root(doc2);

    rc = oas_ref_cache_put(cache, "https://example.com/a.json", doc2, root2);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_size_t(1, oas_ref_cache_count(cache));

    /* Original root is returned */
    yyjson_val *got = oas_ref_cache_get(cache, "https://example.com/a.json");
    TEST_ASSERT_EQUAL_PTR(root1, got);

    /* doc2 was freed by put (cache takes ownership even on dedup) */
    oas_ref_cache_destroy(cache);
}

void test_cache_max_returns_enospc(void)
{
    oas_ref_cache_t *cache = oas_ref_cache_create(2);
    TEST_ASSERT_NOT_NULL(cache);

    yyjson_doc *doc1 = make_doc("{\"n\": 1}");
    yyjson_doc *doc2 = make_doc("{\"n\": 2}");
    yyjson_doc *doc3 = make_doc("{\"n\": 3}");
    TEST_ASSERT_NOT_NULL(doc1);
    TEST_ASSERT_NOT_NULL(doc2);
    TEST_ASSERT_NOT_NULL(doc3);

    int rc = oas_ref_cache_put(cache, "uri://a", doc1, yyjson_doc_get_root(doc1));
    TEST_ASSERT_EQUAL_INT(0, rc);

    rc = oas_ref_cache_put(cache, "uri://b", doc2, yyjson_doc_get_root(doc2));
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* Third insert exceeds max_documents=2 */
    rc = oas_ref_cache_put(cache, "uri://c", doc3, yyjson_doc_get_root(doc3));
    TEST_ASSERT_EQUAL_INT(-ENOSPC, rc);
    TEST_ASSERT_EQUAL_size_t(2, oas_ref_cache_count(cache));

    /* doc3 was not taken by cache, free it manually */
    yyjson_doc_free(doc3);
    oas_ref_cache_destroy(cache);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cache_create_destroy);
    RUN_TEST(test_cache_put_get);
    RUN_TEST(test_cache_miss_returns_null);
    RUN_TEST(test_cache_dedup);
    RUN_TEST(test_cache_max_returns_enospc);
    return UNITY_END();
}
