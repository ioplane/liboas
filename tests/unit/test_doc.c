#include <liboas/oas_doc.h>

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

void test_doc_types_info(void)
{
    oas_info_t *info = oas_arena_alloc(arena, sizeof(*info), _Alignof(oas_info_t));
    TEST_ASSERT_NOT_NULL(info);
    memset(info, 0, sizeof(*info));

    info->title = "Petstore";
    info->version = "1.0.0";
    info->description = "A sample API";

    TEST_ASSERT_EQUAL_STRING("Petstore", info->title);
    TEST_ASSERT_EQUAL_STRING("1.0.0", info->version);
}

void test_doc_types_server(void)
{
    oas_server_t *srv = oas_arena_alloc(arena, sizeof(*srv), _Alignof(oas_server_t));
    TEST_ASSERT_NOT_NULL(srv);
    memset(srv, 0, sizeof(*srv));

    srv->url = "https://api.example.com/v1";
    srv->description = "Production";

    TEST_ASSERT_EQUAL_STRING("https://api.example.com/v1", srv->url);
}

void test_doc_types_path_item(void)
{
    oas_path_item_t *pi = oas_arena_alloc(arena, sizeof(*pi), _Alignof(oas_path_item_t));
    TEST_ASSERT_NOT_NULL(pi);
    memset(pi, 0, sizeof(*pi));

    pi->path = "/pets";
    TEST_ASSERT_NULL(pi->get);
    TEST_ASSERT_NULL(pi->post);
    TEST_ASSERT_EQUAL_STRING("/pets", pi->path);
}

void test_doc_types_operation(void)
{
    oas_operation_t *op = oas_arena_alloc(arena, sizeof(*op), _Alignof(oas_operation_t));
    TEST_ASSERT_NOT_NULL(op);
    memset(op, 0, sizeof(*op));

    op->operation_id = "listPets";
    op->summary = "List all pets";
    op->deprecated = false;

    TEST_ASSERT_EQUAL_STRING("listPets", op->operation_id);
    TEST_ASSERT_FALSE(op->deprecated);
}

void test_doc_types_parameter(void)
{
    oas_parameter_t *p = oas_arena_alloc(arena, sizeof(*p), _Alignof(oas_parameter_t));
    TEST_ASSERT_NOT_NULL(p);
    memset(p, 0, sizeof(*p));

    p->name = "limit";
    p->in = "query";
    p->required = false;

    TEST_ASSERT_EQUAL_STRING("limit", p->name);
    TEST_ASSERT_EQUAL_STRING("query", p->in);
    TEST_ASSERT_FALSE(p->required);
}

void test_doc_types_request_body(void)
{
    oas_request_body_t *rb = oas_arena_alloc(arena, sizeof(*rb), _Alignof(oas_request_body_t));
    TEST_ASSERT_NOT_NULL(rb);
    memset(rb, 0, sizeof(*rb));

    rb->description = "Pet to add";
    rb->required = true;

    TEST_ASSERT_TRUE(rb->required);
    TEST_ASSERT_EQUAL_STRING("Pet to add", rb->description);
}

void test_doc_types_response(void)
{
    oas_response_t *resp = oas_arena_alloc(arena, sizeof(*resp), _Alignof(oas_response_t));
    TEST_ASSERT_NOT_NULL(resp);
    memset(resp, 0, sizeof(*resp));

    resp->description = "A list of pets";
    TEST_ASSERT_EQUAL_STRING("A list of pets", resp->description);
    TEST_ASSERT_EQUAL_UINT64(0, resp->content_count);
}

void test_doc_types_media_type(void)
{
    oas_media_type_t *mt = oas_arena_alloc(arena, sizeof(*mt), _Alignof(oas_media_type_t));
    TEST_ASSERT_NOT_NULL(mt);
    memset(mt, 0, sizeof(*mt));

    mt->media_type_name = "application/json";
    TEST_ASSERT_EQUAL_STRING("application/json", mt->media_type_name);
    TEST_ASSERT_NULL(mt->schema);
}

void test_doc_types_components(void)
{
    oas_components_t *c = oas_arena_alloc(arena, sizeof(*c), _Alignof(oas_components_t));
    TEST_ASSERT_NOT_NULL(c);
    memset(c, 0, sizeof(*c));

    TEST_ASSERT_EQUAL_UINT64(0, c->schemas_count);
    TEST_ASSERT_NULL(c->schemas);
}

void test_doc_types_security_scheme(void)
{
    oas_security_scheme_t *ss =
        oas_arena_alloc(arena, sizeof(*ss), _Alignof(oas_security_scheme_t));
    TEST_ASSERT_NOT_NULL(ss);
    memset(ss, 0, sizeof(*ss));

    ss->type = "http";
    ss->scheme = "bearer";
    ss->bearer_format = "JWT";

    TEST_ASSERT_EQUAL_STRING("http", ss->type);
    TEST_ASSERT_EQUAL_STRING("bearer", ss->scheme);
    TEST_ASSERT_EQUAL_STRING("JWT", ss->bearer_format);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_doc_types_info);
    RUN_TEST(test_doc_types_server);
    RUN_TEST(test_doc_types_path_item);
    RUN_TEST(test_doc_types_operation);
    RUN_TEST(test_doc_types_parameter);
    RUN_TEST(test_doc_types_request_body);
    RUN_TEST(test_doc_types_response);
    RUN_TEST(test_doc_types_media_type);
    RUN_TEST(test_doc_types_components);
    RUN_TEST(test_doc_types_security_scheme);
    return UNITY_END();
}
