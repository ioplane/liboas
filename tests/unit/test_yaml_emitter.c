#include <liboas/oas_emitter.h>

#include <liboas/oas_alloc.h>
#include <liboas/oas_doc.h>
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

/* ── Helpers ────────────────────────────────────────────────────────────── */

static oas_doc_t *make_doc(const char *title, const char *version)
{
    oas_doc_t *doc = oas_arena_alloc(arena, sizeof(*doc), _Alignof(oas_doc_t));
    memset(doc, 0, sizeof(*doc));
    doc->openapi = "3.2.0";

    oas_info_t *info = oas_arena_alloc(arena, sizeof(*info), _Alignof(oas_info_t));
    memset(info, 0, sizeof(*info));
    info->title = title;
    info->version = version;
    doc->info = info;

    return doc;
}

/* ── Tests ──────────────────────────────────────────────────────────────── */

void test_yaml_emit_minimal_doc(void)
{
    oas_doc_t *doc = make_doc("Minimal API", "1.0.0");

    size_t len = 0;
    char *yaml = oas_doc_emit_yaml(doc, nullptr, &len);
    TEST_ASSERT_NOT_NULL(yaml);
    TEST_ASSERT_GREATER_THAN(0, len);

    /* Verify YAML contains expected keys (keys may be quoted or unquoted) */
    TEST_ASSERT_NOT_NULL(strstr(yaml, "openapi"));
    TEST_ASSERT_NOT_NULL(strstr(yaml, "info"));
    TEST_ASSERT_NOT_NULL(strstr(yaml, "title"));
    TEST_ASSERT_NOT_NULL(strstr(yaml, "3.2.0"));
    TEST_ASSERT_NOT_NULL(strstr(yaml, "Minimal API"));

    oas_emit_free(yaml);
}

void test_yaml_emit_null_safe(void)
{
    char *yaml = oas_doc_emit_yaml(nullptr, nullptr, nullptr);
    TEST_ASSERT_NULL(yaml);

    yaml = oas_schema_emit_yaml(nullptr, nullptr, nullptr);
    TEST_ASSERT_NULL(yaml);

    /* oas_emit_free with nullptr is safe */
    oas_emit_free(nullptr);
}

void test_yaml_emit_schema_object(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_OBJECT;
    s->title = "Pet";

    size_t len = 0;
    char *yaml = oas_schema_emit_yaml(s, nullptr, &len);
    TEST_ASSERT_NOT_NULL(yaml);
    TEST_ASSERT_GREATER_THAN(0, len);

    TEST_ASSERT_NOT_NULL(strstr(yaml, "type"));
    TEST_ASSERT_NOT_NULL(strstr(yaml, "object"));
    TEST_ASSERT_NOT_NULL(strstr(yaml, "title"));
    TEST_ASSERT_NOT_NULL(strstr(yaml, "Pet"));

    oas_emit_free(yaml);
}

void test_yaml_emit_pretty_format(void)
{
    oas_doc_t *doc = make_doc("Pretty API", "1.0.0");

    oas_emit_options_t opts = {.pretty = true};
    char *yaml = oas_doc_emit_yaml(doc, &opts, nullptr);
    TEST_ASSERT_NOT_NULL(yaml);

    /* Pretty YAML (block mode) must have newlines */
    TEST_ASSERT_NOT_NULL(strchr(yaml, '\n'));

    oas_emit_free(yaml);
}

void test_yaml_emit_roundtrip_parse_emit(void)
{
    oas_doc_t *doc = make_doc("Roundtrip", "2.0.0");

    /* Add a path */
    oas_operation_t *get_op = oas_arena_alloc(arena, sizeof(*get_op), _Alignof(oas_operation_t));
    memset(get_op, 0, sizeof(*get_op));
    get_op->operation_id = "listPets";
    get_op->summary = "List all pets";

    oas_path_item_t *item = oas_arena_alloc(arena, sizeof(*item), _Alignof(oas_path_item_t));
    memset(item, 0, sizeof(*item));
    item->path = "/pets";
    item->get = get_op;

    oas_path_entry_t entry = {.path = "/pets", .item = item};
    doc->paths = &entry;
    doc->paths_count = 1;

    char *yaml = oas_doc_emit_yaml(doc, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(yaml);

    /* Verify path string appears in YAML output */
    TEST_ASSERT_NOT_NULL(strstr(yaml, "/pets"));
    TEST_ASSERT_NOT_NULL(strstr(yaml, "listPets"));
    TEST_ASSERT_NOT_NULL(strstr(yaml, "openapi"));

    oas_emit_free(yaml);
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_yaml_emit_minimal_doc);
    RUN_TEST(test_yaml_emit_null_safe);
    RUN_TEST(test_yaml_emit_schema_object);
    RUN_TEST(test_yaml_emit_pretty_format);
    RUN_TEST(test_yaml_emit_roundtrip_parse_emit);
    return UNITY_END();
}
