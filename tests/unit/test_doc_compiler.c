#include <liboas/oas_compiler.h>
#include <liboas/oas_doc.h>
#include <liboas/oas_error.h>
#include <liboas/oas_regex.h>
#include <liboas/oas_schema.h>

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

/* Helper: build minimal doc with one POST operation */
static oas_doc_t *build_doc_with_schema(oas_schema_t *req_schema)
{
    oas_doc_t *doc = oas_arena_alloc(arena, sizeof(*doc), _Alignof(oas_doc_t));
    memset(doc, 0, sizeof(*doc));
    doc->openapi = "3.2.0";

    oas_info_t *info = oas_arena_alloc(arena, sizeof(*info), _Alignof(oas_info_t));
    memset(info, 0, sizeof(*info));
    info->title = "Test";
    info->version = "1.0";
    doc->info = info;

    oas_operation_t *op = oas_arena_alloc(arena, sizeof(*op), _Alignof(oas_operation_t));
    memset(op, 0, sizeof(*op));

    if (req_schema) {
        oas_request_body_t *rb = oas_arena_alloc(arena, sizeof(*rb), _Alignof(oas_request_body_t));
        memset(rb, 0, sizeof(*rb));
        rb->required = true;

        oas_media_type_t *mt = oas_arena_alloc(arena, sizeof(*mt), _Alignof(oas_media_type_t));
        mt->media_type_name = "application/json";
        mt->schema = req_schema;

        oas_media_type_entry_t *mte =
            oas_arena_alloc(arena, sizeof(*mte), _Alignof(oas_media_type_entry_t));
        mte->key = "application/json";
        mte->value = mt;

        rb->content = mte;
        rb->content_count = 1;
        op->request_body = rb;
    }

    /* Response 200 with no body */
    oas_response_t *resp = oas_arena_alloc(arena, sizeof(*resp), _Alignof(oas_response_t));
    memset(resp, 0, sizeof(*resp));
    resp->description = "ok";

    oas_response_entry_t *re = oas_arena_alloc(arena, sizeof(*re), _Alignof(oas_response_entry_t));
    re->status_code = "200";
    re->response = resp;
    op->responses = re;
    op->responses_count = 1;

    oas_path_item_t *item = oas_arena_alloc(arena, sizeof(*item), _Alignof(oas_path_item_t));
    memset(item, 0, sizeof(*item));
    item->path = "/test";
    item->post = op;

    oas_path_entry_t *pe = oas_arena_alloc(arena, sizeof(*pe), _Alignof(oas_path_entry_t));
    pe->path = "/test";
    pe->item = item;
    doc->paths = pe;
    doc->paths_count = 1;

    return doc;
}

/* ── Bug 2 fix: compile failure propagation ─────────────────────────────── */

void test_doc_compile_valid_schema_succeeds(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;

    oas_doc_t *doc = build_doc_with_schema(s);
    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, errors);
    TEST_ASSERT_NOT_NULL(cdoc);
    TEST_ASSERT_FALSE(oas_error_list_has_errors(errors));

    oas_compiled_doc_free(cdoc);
}

void test_doc_compile_unresolved_ref_fails(void)
{
    /* Schema with $ref but no ref_resolved — compiler cannot compile it */
    oas_schema_t *s = oas_schema_create(arena);
    s->ref = "#/components/schemas/Missing";
    /* ref_resolved left as nullptr */

    oas_doc_t *doc = build_doc_with_schema(s);
    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, errors);
    TEST_ASSERT_NULL_MESSAGE(cdoc, "should fail when schema compilation fails");
}

void test_doc_compile_accumulates_errors(void)
{
    /* Two media types, both with unresolved refs */
    oas_doc_t *doc = oas_arena_alloc(arena, sizeof(*doc), _Alignof(oas_doc_t));
    memset(doc, 0, sizeof(*doc));
    doc->openapi = "3.2.0";

    oas_info_t *info = oas_arena_alloc(arena, sizeof(*info), _Alignof(oas_info_t));
    memset(info, 0, sizeof(*info));
    info->title = "Test";
    info->version = "1.0";
    doc->info = info;

    oas_operation_t *op = oas_arena_alloc(arena, sizeof(*op), _Alignof(oas_operation_t));
    memset(op, 0, sizeof(*op));

    oas_request_body_t *rb = oas_arena_alloc(arena, sizeof(*rb), _Alignof(oas_request_body_t));
    memset(rb, 0, sizeof(*rb));

    oas_media_type_entry_t *mtes =
        oas_arena_alloc(arena, 2 * sizeof(*mtes), _Alignof(oas_media_type_entry_t));

    oas_schema_t *s1 = oas_schema_create(arena);
    s1->ref = "#/components/schemas/A";

    oas_schema_t *s2 = oas_schema_create(arena);
    s2->ref = "#/components/schemas/B";

    oas_media_type_t *mt1 = oas_arena_alloc(arena, sizeof(*mt1), _Alignof(oas_media_type_t));
    memset(mt1, 0, sizeof(*mt1));
    mt1->media_type_name = "application/json";
    mt1->schema = s1;
    mtes[0].key = "application/json";
    mtes[0].value = mt1;

    oas_media_type_t *mt2 = oas_arena_alloc(arena, sizeof(*mt2), _Alignof(oas_media_type_t));
    memset(mt2, 0, sizeof(*mt2));
    mt2->media_type_name = "application/xml";
    mt2->schema = s2;
    mtes[1].key = "application/xml";
    mtes[1].value = mt2;

    rb->content = mtes;
    rb->content_count = 2;
    op->request_body = rb;

    oas_response_t *resp = oas_arena_alloc(arena, sizeof(*resp), _Alignof(oas_response_t));
    memset(resp, 0, sizeof(*resp));
    resp->description = "ok";
    oas_response_entry_t *re = oas_arena_alloc(arena, sizeof(*re), _Alignof(oas_response_entry_t));
    re->status_code = "200";
    re->response = resp;
    op->responses = re;
    op->responses_count = 1;

    oas_path_item_t *item = oas_arena_alloc(arena, sizeof(*item), _Alignof(oas_path_item_t));
    memset(item, 0, sizeof(*item));
    item->path = "/test";
    item->post = op;

    oas_path_entry_t *pe = oas_arena_alloc(arena, sizeof(*pe), _Alignof(oas_path_entry_t));
    pe->path = "/test";
    pe->item = item;
    doc->paths = pe;
    doc->paths_count = 1;

    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, errors);
    TEST_ASSERT_NULL_MESSAGE(cdoc, "should fail when multiple schemas fail to compile");
}

/* ── Bad regex pattern triggers compile failure ─────────────────────────── */

void test_compile_fails_on_bad_schema(void)
{
    /* Schema with an invalid regex pattern triggers compile failure */
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING;
    s->pattern = "[invalid";

    oas_doc_t *doc = build_doc_with_schema(s);

    oas_regex_backend_t *regex = oas_regex_libregexp_create();
    TEST_ASSERT_NOT_NULL(regex);

    oas_compiler_config_t config = {.regex = regex, .borrow_regex = true};
    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, &config, errors);
    TEST_ASSERT_NULL_MESSAGE(cdoc, "should return NULL when schema has invalid regex");
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));

    regex->destroy(regex);
}

void test_compile_error_propagates_to_doc(void)
{
    /* oas_doc_compile returns NULL when a media type schema fails to compile */
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING;
    s->pattern = "(unclosed";

    oas_doc_t *doc = build_doc_with_schema(s);

    oas_regex_backend_t *regex = oas_regex_libregexp_create();
    TEST_ASSERT_NOT_NULL(regex);

    oas_compiler_config_t config = {.regex = regex, .borrow_regex = true};
    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, &config, errors);
    TEST_ASSERT_NULL_MESSAGE(cdoc, "doc compile must propagate schema failure to caller");

    /* Error list should contain the regex compilation error */
    size_t count = oas_error_list_count(errors);
    TEST_ASSERT_GREATER_THAN_size_t(0, count);

    const oas_error_t *err = oas_error_list_get(errors, 0);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(OAS_ERR_CONSTRAINT, err->kind);

    regex->destroy(regex);
}

void test_compile_partial_failure_accumulates_errors(void)
{
    /* Two schemas: one valid, one with invalid regex — all errors collected, returns NULL */
    oas_doc_t *doc = oas_arena_alloc(arena, sizeof(*doc), _Alignof(oas_doc_t));
    memset(doc, 0, sizeof(*doc));
    doc->openapi = "3.2.0";

    oas_info_t *info = oas_arena_alloc(arena, sizeof(*info), _Alignof(oas_info_t));
    memset(info, 0, sizeof(*info));
    info->title = "Test";
    info->version = "1.0";
    doc->info = info;

    oas_operation_t *op = oas_arena_alloc(arena, sizeof(*op), _Alignof(oas_operation_t));
    memset(op, 0, sizeof(*op));

    oas_request_body_t *rb = oas_arena_alloc(arena, sizeof(*rb), _Alignof(oas_request_body_t));
    memset(rb, 0, sizeof(*rb));

    oas_media_type_entry_t *mtes =
        oas_arena_alloc(arena, 2 * sizeof(*mtes), _Alignof(oas_media_type_entry_t));

    /* First schema: valid */
    oas_schema_t *s1 = oas_schema_create(arena);
    s1->type_mask = OAS_TYPE_STRING;
    s1->pattern = "^[a-z]+$";

    /* Second schema: invalid regex */
    oas_schema_t *s2 = oas_schema_create(arena);
    s2->type_mask = OAS_TYPE_STRING;
    s2->pattern = "[broken";

    oas_media_type_t *mt1 = oas_arena_alloc(arena, sizeof(*mt1), _Alignof(oas_media_type_t));
    memset(mt1, 0, sizeof(*mt1));
    mt1->media_type_name = "application/json";
    mt1->schema = s1;
    mtes[0].key = "application/json";
    mtes[0].value = mt1;

    oas_media_type_t *mt2 = oas_arena_alloc(arena, sizeof(*mt2), _Alignof(oas_media_type_t));
    memset(mt2, 0, sizeof(*mt2));
    mt2->media_type_name = "application/xml";
    mt2->schema = s2;
    mtes[1].key = "application/xml";
    mtes[1].value = mt2;

    rb->content = mtes;
    rb->content_count = 2;
    op->request_body = rb;

    oas_response_t *resp = oas_arena_alloc(arena, sizeof(*resp), _Alignof(oas_response_t));
    memset(resp, 0, sizeof(*resp));
    resp->description = "ok";
    oas_response_entry_t *re = oas_arena_alloc(arena, sizeof(*re), _Alignof(oas_response_entry_t));
    re->status_code = "200";
    re->response = resp;
    op->responses = re;
    op->responses_count = 1;

    oas_path_item_t *item = oas_arena_alloc(arena, sizeof(*item), _Alignof(oas_path_item_t));
    memset(item, 0, sizeof(*item));
    item->path = "/test";
    item->post = op;

    oas_path_entry_t *pe = oas_arena_alloc(arena, sizeof(*pe), _Alignof(oas_path_entry_t));
    pe->path = "/test";
    pe->item = item;
    doc->paths = pe;
    doc->paths_count = 1;

    oas_regex_backend_t *regex = oas_regex_libregexp_create();
    TEST_ASSERT_NOT_NULL(regex);

    oas_compiler_config_t config = {.regex = regex, .borrow_regex = true};
    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, &config, errors);
    TEST_ASSERT_NULL_MESSAGE(cdoc, "partial failure must still return NULL");
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));

    /* Should have at least one error for the bad regex */
    TEST_ASSERT_GREATER_THAN_size_t(0, oas_error_list_count(errors));

    regex->destroy(regex);
}

/* ── Bug 3 fix: regex backend ownership ─────────────────────────────────── */

void test_doc_compile_owns_regex_by_default(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING;
    s->pattern = "^[a-z]+$";

    oas_doc_t *doc = build_doc_with_schema(s);

    oas_regex_backend_t *regex = oas_regex_libregexp_create();
    TEST_ASSERT_NOT_NULL(regex);

    oas_compiler_config_t config = {.regex = regex, .format_policy = 0};
    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, &config, errors);
    TEST_ASSERT_NOT_NULL(cdoc);

    /* Freeing cdoc frees regex (default ownership) — no crash = success */
    oas_compiled_doc_free(cdoc);
}

void test_doc_compile_borrowed_regex_not_freed(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING;
    s->pattern = "^[0-9]+$";

    oas_doc_t *doc = build_doc_with_schema(s);

    oas_regex_backend_t *regex = oas_regex_libregexp_create();
    TEST_ASSERT_NOT_NULL(regex);

    oas_compiler_config_t config = {
        .regex = regex,
        .format_policy = 0,
        .borrow_regex = true, /* caller retains ownership */
    };
    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, &config, errors);
    TEST_ASSERT_NOT_NULL(cdoc);

    oas_compiled_doc_free(cdoc);

    /* regex should still be alive — compile a second doc to prove it */
    oas_doc_t *doc2 = build_doc_with_schema(s);
    oas_compiled_doc_t *cdoc2 = oas_doc_compile(doc2, &config, errors);
    TEST_ASSERT_NOT_NULL(cdoc2);

    oas_compiled_doc_free(cdoc2);
    regex->destroy(regex); /* caller frees */
}

void test_doc_compile_two_docs_separate_regex(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING;
    s->pattern = "^test$";

    oas_doc_t *doc1 = build_doc_with_schema(s);
    oas_doc_t *doc2 = build_doc_with_schema(s);

    oas_regex_backend_t *r1 = oas_regex_libregexp_create();
    oas_regex_backend_t *r2 = oas_regex_libregexp_create();

    oas_compiler_config_t c1 = {.regex = r1};
    oas_compiler_config_t c2 = {.regex = r2};

    oas_compiled_doc_t *cd1 = oas_doc_compile(doc1, &c1, errors);
    oas_compiled_doc_t *cd2 = oas_doc_compile(doc2, &c2, errors);
    TEST_ASSERT_NOT_NULL(cd1);
    TEST_ASSERT_NOT_NULL(cd2);

    /* Free independently — no double-free or use-after-free */
    oas_compiled_doc_free(cd1);
    oas_compiled_doc_free(cd2);
}

int main(void)
{
    UNITY_BEGIN();
    /* Compile failure propagation */
    RUN_TEST(test_doc_compile_valid_schema_succeeds);
    RUN_TEST(test_doc_compile_unresolved_ref_fails);
    RUN_TEST(test_doc_compile_accumulates_errors);
    /* Bad regex pattern failures */
    RUN_TEST(test_compile_fails_on_bad_schema);
    RUN_TEST(test_compile_error_propagates_to_doc);
    RUN_TEST(test_compile_partial_failure_accumulates_errors);
    /* Regex backend ownership */
    RUN_TEST(test_doc_compile_owns_regex_by_default);
    RUN_TEST(test_doc_compile_borrowed_regex_not_freed);
    RUN_TEST(test_doc_compile_two_docs_separate_regex);
    return UNITY_END();
}
