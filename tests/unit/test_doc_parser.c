#include <liboas/oas_parser.h>

#include <string.h>
#include <unity.h>

static oas_arena_t *arena;
static oas_error_list_t *errors;
static oas_doc_t *parsed_doc;

void setUp(void)
{
    arena = oas_arena_create(0);
    errors = oas_error_list_create(arena);
    parsed_doc = nullptr;
}

void tearDown(void)
{
    oas_doc_free(parsed_doc);
    oas_arena_destroy(arena);
    arena = nullptr;
    errors = nullptr;
    parsed_doc = nullptr;
}

static oas_doc_t *parse_str(const char *json)
{
    parsed_doc = oas_doc_parse(arena, json, strlen(json), errors);
    return parsed_doc;
}

void test_doc_parse_minimal(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_STRING("3.2.0", doc->openapi);
    TEST_ASSERT_NOT_NULL(doc->info);
    TEST_ASSERT_EQUAL_STRING("T", doc->info->title);
    TEST_ASSERT_EQUAL_STRING("1", doc->info->version);
}

void test_doc_parse_petstore(void)
{
    oas_doc_t *doc = oas_doc_parse_file(arena, "tests/fixtures/petstore.json", errors);
    parsed_doc = doc;
    TEST_ASSERT_NOT_NULL_MESSAGE(doc, "petstore.json should parse successfully");
    TEST_ASSERT_EQUAL_STRING("3.2.0", doc->openapi);
    TEST_ASSERT_EQUAL_STRING("Petstore", doc->info->title);
    TEST_ASSERT_TRUE(doc->paths_count >= 2);
    TEST_ASSERT_NOT_NULL(doc->components);
    TEST_ASSERT_TRUE(doc->components->schemas_count >= 1);
}

void test_doc_parse_invalid_json(void)
{
    oas_doc_t *doc = parse_str("{not valid json}");
    TEST_ASSERT_NULL(doc);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));
}

void test_doc_parse_missing_openapi(void)
{
    oas_doc_t *doc = parse_str("{\"info\": {\"title\": \"T\", \"version\": \"1\"}}");
    TEST_ASSERT_NULL(doc);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));
}

void test_doc_parse_wrong_version(void)
{
    oas_doc_t *doc =
        parse_str("{\"openapi\": \"2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}}");
    TEST_ASSERT_NULL(doc);
    TEST_ASSERT_TRUE(oas_error_list_has_errors(errors));
}

void test_doc_parse_info_fields(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"My API\", "
                       "\"description\": \"A test\", \"version\": \"2.0.0\", "
                       "\"contact\": {\"name\": \"Dev\", \"email\": \"dev@test.com\"}, "
                       "\"license\": {\"name\": \"MIT\"}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_STRING("My API", doc->info->title);
    TEST_ASSERT_EQUAL_STRING("A test", doc->info->description);
    TEST_ASSERT_NOT_NULL(doc->info->contact);
    TEST_ASSERT_EQUAL_STRING("Dev", doc->info->contact->name);
    TEST_ASSERT_NOT_NULL(doc->info->license);
    TEST_ASSERT_EQUAL_STRING("MIT", doc->info->license->name);
}

void test_doc_parse_servers(void)
{
    const char *json =
        "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
        "\"servers\": [{\"url\": \"https://api.test.com\", \"description\": \"Prod\"}]}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_UINT64(1, doc->servers_count);
    TEST_ASSERT_EQUAL_STRING("https://api.test.com", doc->servers[0]->url);
    TEST_ASSERT_EQUAL_STRING("Prod", doc->servers[0]->description);
}

void test_doc_parse_paths(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"paths\": {\"/pets\": {\"get\": {\"operationId\": \"listPets\", "
                       "\"responses\": {\"200\": {\"description\": \"OK\"}}}}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_UINT64(1, doc->paths_count);
    TEST_ASSERT_EQUAL_STRING("/pets", doc->paths[0].path);
    TEST_ASSERT_NOT_NULL(doc->paths[0].item);
    TEST_ASSERT_NOT_NULL(doc->paths[0].item->get);
    TEST_ASSERT_EQUAL_STRING("listPets", doc->paths[0].item->get->operation_id);
}

void test_doc_parse_operations(void)
{
    const char *json =
        "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
        "\"paths\": {\"/pets\": {"
        "\"get\": {\"operationId\": \"list\", \"summary\": \"List\", "
        "\"tags\": [\"pets\"], \"responses\": {\"200\": {\"description\": \"OK\"}}}, "
        "\"post\": {\"operationId\": \"create\", \"deprecated\": true, "
        "\"responses\": {\"201\": {\"description\": \"Created\"}}}"
        "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);

    oas_path_item_t *pi = doc->paths[0].item;
    TEST_ASSERT_NOT_NULL(pi->get);
    TEST_ASSERT_NOT_NULL(pi->post);
    TEST_ASSERT_NULL(pi->put);

    TEST_ASSERT_EQUAL_STRING("list", pi->get->operation_id);
    TEST_ASSERT_EQUAL_UINT64(1, pi->get->tags_count);
    TEST_ASSERT_EQUAL_STRING("pets", pi->get->tags[0]);

    TEST_ASSERT_TRUE(pi->post->deprecated);
}

void test_doc_parse_components_schemas(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"components\": {\"schemas\": {"
                       "\"Pet\": {\"type\": \"object\", \"properties\": {"
                       "\"name\": {\"type\": \"string\"}}}"
                       "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_NOT_NULL(doc->components);
    TEST_ASSERT_EQUAL_UINT64(1, doc->components->schemas_count);
    TEST_ASSERT_EQUAL_STRING("Pet", doc->components->schemas[0].name);
    TEST_ASSERT_NOT_NULL(doc->components->schemas[0].schema);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_OBJECT, doc->components->schemas[0].schema->type_mask);
}

/* ── Sprint 11: Security requirements ────────────────────────────────── */

void test_parse_root_security(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"security\": [{\"bearerAuth\": []}]}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_size_t(1, doc->security_count);
    TEST_ASSERT_NOT_NULL(doc->security[0]);
    TEST_ASSERT_EQUAL_STRING("bearerAuth", doc->security[0]->name);
    TEST_ASSERT_EQUAL_size_t(0, doc->security[0]->scopes_count);
}

void test_parse_security_with_scopes(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"security\": [{\"oauth2\": [\"read:pets\", \"write:pets\"]}]}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_size_t(1, doc->security_count);
    TEST_ASSERT_NOT_NULL(doc->security[0]);
    TEST_ASSERT_EQUAL_STRING("oauth2", doc->security[0]->name);
    TEST_ASSERT_EQUAL_size_t(2, doc->security[0]->scopes_count);
    TEST_ASSERT_EQUAL_STRING("read:pets", doc->security[0]->scopes[0]);
    TEST_ASSERT_EQUAL_STRING("write:pets", doc->security[0]->scopes[1]);
}

void test_parse_operation_security(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"paths\": {\"/pets\": {\"get\": {\"operationId\": \"list\", "
                       "\"security\": [{\"apiKey\": []}], "
                       "\"responses\": {\"200\": {\"description\": \"OK\"}}}}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    oas_operation_t *op = doc->paths[0].item->get;
    TEST_ASSERT_EQUAL_size_t(1, op->security_count);
    TEST_ASSERT_NOT_NULL(op->security[0]);
    TEST_ASSERT_EQUAL_STRING("apiKey", op->security[0]->name);
}

void test_parse_empty_security(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"security\": [{}]}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_size_t(1, doc->security_count);
    TEST_ASSERT_NULL(doc->security[0]); /* empty = anonymous */
}

/* ── Sprint 11: Security schemes ─────────────────────────────────────── */

void test_parse_security_scheme_apikey(void)
{
    const char *json =
        "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
        "\"components\": {\"securitySchemes\": {"
        "\"apiKey\": {\"type\": \"apiKey\", \"name\": \"X-API-Key\", \"in\": \"header\"}"
        "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_NOT_NULL(doc->components);
    TEST_ASSERT_EQUAL_size_t(1, doc->components->security_schemes_count);
    oas_security_scheme_t *ss = doc->components->security_schemes[0].scheme;
    TEST_ASSERT_NOT_NULL(ss);
    TEST_ASSERT_EQUAL_STRING("apiKey", ss->type);
    TEST_ASSERT_EQUAL_STRING("X-API-Key", ss->name);
    TEST_ASSERT_EQUAL_STRING("header", ss->in);
}

void test_parse_security_scheme_http_bearer(void)
{
    const char *json =
        "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
        "\"components\": {\"securitySchemes\": {"
        "\"bearer\": {\"type\": \"http\", \"scheme\": \"bearer\", \"bearerFormat\": \"JWT\"}"
        "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    oas_security_scheme_t *ss = doc->components->security_schemes[0].scheme;
    TEST_ASSERT_NOT_NULL(ss);
    TEST_ASSERT_EQUAL_STRING("http", ss->type);
    TEST_ASSERT_EQUAL_STRING("bearer", ss->scheme);
    TEST_ASSERT_EQUAL_STRING("JWT", ss->bearer_format);
}

void test_parse_security_scheme_oauth2(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"components\": {\"securitySchemes\": {"
                       "\"oauth2\": {\"type\": \"oauth2\", \"flows\": {"
                       "\"authorizationCode\": {"
                       "\"authorizationUrl\": \"https://auth.test.com/authorize\", "
                       "\"tokenUrl\": \"https://auth.test.com/token\", "
                       "\"scopes\": {\"read:pets\": \"Read pets\", \"write:pets\": \"Write pets\"}"
                       "}}}}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    oas_security_scheme_t *ss = doc->components->security_schemes[0].scheme;
    TEST_ASSERT_NOT_NULL(ss);
    TEST_ASSERT_EQUAL_STRING("oauth2", ss->type);
    TEST_ASSERT_NOT_NULL(ss->flows);
    TEST_ASSERT_NOT_NULL(ss->flows->authorization_code);
    TEST_ASSERT_EQUAL_STRING("https://auth.test.com/authorize",
                             ss->flows->authorization_code->authorization_url);
    TEST_ASSERT_EQUAL_STRING("https://auth.test.com/token",
                             ss->flows->authorization_code->token_url);
    TEST_ASSERT_EQUAL_size_t(2, ss->flows->authorization_code->scopes_count);
    TEST_ASSERT_EQUAL_STRING("read:pets", ss->flows->authorization_code->scope_names[0]);
}

void test_parse_security_scheme_openid(void)
{
    const char *json =
        "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
        "\"components\": {\"securitySchemes\": {"
        "\"oidc\": {\"type\": \"openIdConnect\", "
        "\"openIdConnectUrl\": \"https://auth.test.com/.well-known/openid-configuration\"}"
        "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    oas_security_scheme_t *ss = doc->components->security_schemes[0].scheme;
    TEST_ASSERT_NOT_NULL(ss);
    TEST_ASSERT_EQUAL_STRING("openIdConnect", ss->type);
    TEST_ASSERT_EQUAL_STRING("https://auth.test.com/.well-known/openid-configuration",
                             ss->open_id_connect_url);
}

/* ── Sprint 11: Server variables ─────────────────────────────────────── */

void test_parse_server_variables(void)
{
    const char *json =
        "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
        "\"servers\": [{\"url\": \"https://{host}:{port}/v1\", "
        "\"variables\": {\"port\": {\"default\": \"8080\", \"enum\": [\"80\", \"8080\"]}}}]}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_size_t(1, doc->servers_count);
    TEST_ASSERT_EQUAL_size_t(1, doc->servers[0]->variables_count);
    oas_server_var_t *sv = doc->servers[0]->variables[0];
    TEST_ASSERT_NOT_NULL(sv);
    TEST_ASSERT_EQUAL_STRING("port", sv->name);
    TEST_ASSERT_EQUAL_STRING("8080", sv->default_value);
    TEST_ASSERT_EQUAL_size_t(2, sv->enum_count);
    TEST_ASSERT_EQUAL_STRING("80", sv->enum_values[0]);
    TEST_ASSERT_EQUAL_STRING("8080", sv->enum_values[1]);
}

void test_parse_server_variables_description(void)
{
    const char *json =
        "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
        "\"servers\": [{\"url\": \"https://api.test.com\", "
        "\"variables\": {\"env\": {\"default\": \"prod\", \"description\": \"Environment\"}}}]}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    oas_server_var_t *sv = doc->servers[0]->variables[0];
    TEST_ASSERT_EQUAL_STRING("Environment", sv->description);
}

void test_parse_server_no_variables(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"servers\": [{\"url\": \"https://api.test.com\"}]}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_size_t(0, doc->servers[0]->variables_count);
}

/* ── Sprint 11: Component types ──────────────────────────────────────── */

void test_parse_component_responses(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"components\": {\"responses\": {"
                       "\"NotFound\": {\"description\": \"Resource not found\"}"
                       "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_NOT_NULL(doc->components);
    TEST_ASSERT_EQUAL_size_t(1, doc->components->responses_count);
    TEST_ASSERT_EQUAL_STRING("NotFound", doc->components->responses[0].name);
    TEST_ASSERT_NOT_NULL(doc->components->responses[0].response);
    TEST_ASSERT_EQUAL_STRING("Resource not found",
                             doc->components->responses[0].response->description);
}

void test_parse_component_parameters(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"components\": {\"parameters\": {"
                       "\"PageSize\": {\"name\": \"limit\", \"in\": \"query\", "
                       "\"schema\": {\"type\": \"integer\"}}"
                       "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_size_t(1, doc->components->parameters_count);
    TEST_ASSERT_EQUAL_STRING("PageSize", doc->components->parameters[0].name);
    oas_parameter_t *p = doc->components->parameters[0].parameter;
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_STRING("limit", p->name);
    TEST_ASSERT_EQUAL_STRING("query", p->in);
    TEST_ASSERT_NOT_NULL(p->schema);
}

void test_parse_component_request_bodies(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"components\": {\"requestBodies\": {"
                       "\"PetBody\": {\"required\": true, \"content\": {"
                       "\"application/json\": {\"schema\": {\"type\": \"object\"}}}}"
                       "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_size_t(1, doc->components->request_bodies_count);
    TEST_ASSERT_EQUAL_STRING("PetBody", doc->components->request_bodies[0].name);
    oas_request_body_t *rb = doc->components->request_bodies[0].request_body;
    TEST_ASSERT_NOT_NULL(rb);
    TEST_ASSERT_TRUE(rb->required);
    TEST_ASSERT_EQUAL_size_t(1, rb->content_count);
}

void test_parse_component_headers(void)
{
    const char *json = "{\"openapi\": \"3.2.0\", \"info\": {\"title\": \"T\", \"version\": \"1\"}, "
                       "\"components\": {\"headers\": {"
                       "\"X-Rate-Limit\": {\"description\": \"Rate limit\", "
                       "\"schema\": {\"type\": \"integer\"}}"
                       "}}}";
    oas_doc_t *doc = parse_str(json);
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_size_t(1, doc->components->headers_count);
    TEST_ASSERT_EQUAL_STRING("X-Rate-Limit", doc->components->headers[0].name);
    oas_parameter_t *h = doc->components->headers[0].header;
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_EQUAL_STRING("Rate limit", h->description);
    TEST_ASSERT_NOT_NULL(h->schema);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_doc_parse_minimal);
    RUN_TEST(test_doc_parse_petstore);
    RUN_TEST(test_doc_parse_invalid_json);
    RUN_TEST(test_doc_parse_missing_openapi);
    RUN_TEST(test_doc_parse_wrong_version);
    RUN_TEST(test_doc_parse_info_fields);
    RUN_TEST(test_doc_parse_servers);
    RUN_TEST(test_doc_parse_paths);
    RUN_TEST(test_doc_parse_operations);
    RUN_TEST(test_doc_parse_components_schemas);
    /* Security requirements */
    RUN_TEST(test_parse_root_security);
    RUN_TEST(test_parse_security_with_scopes);
    RUN_TEST(test_parse_operation_security);
    RUN_TEST(test_parse_empty_security);
    /* Security schemes */
    RUN_TEST(test_parse_security_scheme_apikey);
    RUN_TEST(test_parse_security_scheme_http_bearer);
    RUN_TEST(test_parse_security_scheme_oauth2);
    RUN_TEST(test_parse_security_scheme_openid);
    /* Server variables */
    RUN_TEST(test_parse_server_variables);
    RUN_TEST(test_parse_server_variables_description);
    RUN_TEST(test_parse_server_no_variables);
    /* Component types */
    RUN_TEST(test_parse_component_responses);
    RUN_TEST(test_parse_component_parameters);
    RUN_TEST(test_parse_component_request_bodies);
    RUN_TEST(test_parse_component_headers);
    return UNITY_END();
}
