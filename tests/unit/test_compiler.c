#include <liboas/oas_compiler.h>
#include <liboas/oas_regex.h>
#include <liboas/oas_schema.h>

#include "compiler/oas_format.h"
#include "compiler/oas_instruction.h"

#include <string.h>
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

/* Helper: get the program from a compiled schema for inspection */
static const oas_instruction_t *get_code(const oas_compiled_schema_t *cs)
{
    /* compiled_schema's first field is oas_program_t which has code as first field */
    const oas_program_t *prog = (const oas_program_t *)cs;
    return prog->code;
}

static size_t get_count(const oas_compiled_schema_t *cs)
{
    const oas_program_t *prog = (const oas_program_t *)cs;
    return prog->count;
}

/* ── Test 1: empty schema ──────────────────────────────────────────────── */

void test_compiler_empty_schema(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    TEST_ASSERT_NOT_NULL(s);

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_ASSERT_EQUAL_UINT64(1, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, get_code(cs)[0].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 2: type string ───────────────────────────────────────────────── */

void test_compiler_type_string(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_ASSERT_EQUAL_UINT64(2, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, get_code(cs)[0].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, get_code(cs)[0].type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, get_code(cs)[1].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 3: multiple types ────────────────────────────────────────────── */

void test_compiler_type_array_multiple(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->type_mask = OAS_TYPE_STRING | OAS_TYPE_INTEGER;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_ASSERT_EQUAL_UINT64(2, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, get_code(cs)[0].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING | OAS_TYPE_INTEGER,
                            get_code(cs)[0].type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, get_code(cs)[1].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 4: string constraints ────────────────────────────────────────── */

void test_compiler_string_constraints(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->min_length = 1;
    s->max_length = 100;
    s->format = "email";

    oas_compiler_config_t config = {.regex = regex, .format_policy = OAS_FORMAT_ENFORCE};
    oas_compiled_schema_t *cs = oas_schema_compile(s, &config, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    size_t count = get_count(cs);

    TEST_ASSERT_EQUAL_UINT64(4, count);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_MIN_LEN, code[0].op);
    TEST_ASSERT_EQUAL_INT64(1, code[0].operand.i64);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_MAX_LEN, code[1].op);
    TEST_ASSERT_EQUAL_INT64(100, code[1].operand.i64);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_FORMAT, code[2].op);
    TEST_ASSERT_NOT_NULL(code[2].operand.ptr);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[3].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 5: integer range ─────────────────────────────────────────────── */

void test_compiler_integer_range(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->has_minimum = true;
    s->minimum = 0.0;
    s->has_maximum = true;
    s->maximum = 100.0;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    TEST_ASSERT_EQUAL_UINT64(3, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_MINIMUM, code[0].op);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, code[0].operand.f64);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_MAXIMUM, code[1].op);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, code[1].operand.f64);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[2].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 6: multiple_of ───────────────────────────────────────────────── */

void test_compiler_multiple_of(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->has_multiple_of = true;
    s->multiple_of = 5.0;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    TEST_ASSERT_EQUAL_UINT64(2, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_MULTIPLE_OF, code[0].op);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, code[0].operand.f64);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[1].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 7: array items ───────────────────────────────────────────────── */

void test_compiler_array_items(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *item = oas_schema_create(arena);
    item->type_mask = OAS_TYPE_STRING;
    s->items = item;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    /* ENTER_ITEMS, CHECK_TYPE, END (items sub), END (program) */
    TEST_ASSERT_EQUAL_UINT64(4, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_ENTER_ITEMS, code[0].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[1].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, code[1].type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[2].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[3].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 8: prefix items ──────────────────────────────────────────────── */

void test_compiler_array_prefix_items(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *p0 = oas_schema_create(arena);
    p0->type_mask = OAS_TYPE_STRING;
    oas_schema_t *p1 = oas_schema_create(arena);
    p1->type_mask = OAS_TYPE_INTEGER;

    oas_schema_t *items[2] = {p0, p1};
    s->prefix_items = items;
    s->prefix_items_count = 2;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    /* PREFIX_ITEM(0), CHECK_TYPE(string), END, PREFIX_ITEM(1), CHECK_TYPE(int), END, END */
    TEST_ASSERT_EQUAL_UINT64(7, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_ENTER_PREFIX_ITEM, code[0].op);
    TEST_ASSERT_EQUAL_UINT16(0, code[0].operand.branch.index);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[1].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[2].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_ENTER_PREFIX_ITEM, code[3].op);
    TEST_ASSERT_EQUAL_UINT16(1, code[3].operand.branch.index);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[4].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[5].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[6].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 9: object required ───────────────────────────────────────────── */

void test_compiler_object_required(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    const char *req[] = {"name", "id"};
    s->required = req;
    s->required_count = 2;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    TEST_ASSERT_EQUAL_UINT64(3, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_REQUIRED, code[0].op);
    TEST_ASSERT_EQUAL_STRING("name", code[0].operand.str);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_REQUIRED, code[1].op);
    TEST_ASSERT_EQUAL_STRING("id", code[1].operand.str);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[2].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 10: object properties ────────────────────────────────────────── */

void test_compiler_object_properties(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *name_s = oas_schema_create(arena);
    name_s->type_mask = OAS_TYPE_STRING;
    oas_schema_t *age_s = oas_schema_create(arena);
    age_s->type_mask = OAS_TYPE_INTEGER;

    int rc = oas_schema_add_property(arena, s, "name", name_s);
    TEST_ASSERT_EQUAL_INT(0, rc);
    rc = oas_schema_add_property(arena, s, "age", age_s);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    /* Properties are added as a linked list (prepend), so "age" comes first */
    /* ENTER_PROPERTY("age"), CHECK_TYPE(int), END,
       ENTER_PROPERTY("name"), CHECK_TYPE(str), END, END */
    TEST_ASSERT_EQUAL_UINT64(7, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_ENTER_PROPERTY, code[0].op);
    TEST_ASSERT_EQUAL_STRING("age", code[0].operand.str);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[1].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[2].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_ENTER_PROPERTY, code[3].op);
    TEST_ASSERT_EQUAL_STRING("name", code[3].operand.str);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[4].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[5].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[6].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 11: allOf ────────────────────────────────────────────────────── */

void test_compiler_allof(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *s1 = oas_schema_create(arena);
    s1->type_mask = OAS_TYPE_OBJECT;
    oas_schema_t *s2 = oas_schema_create(arena);
    s2->type_mask = OAS_TYPE_STRING;

    oas_schema_t *branches[2] = {s1, s2};
    s->all_of = branches;
    s->all_of_count = 2;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    /* BRANCH_ALLOF(2), CHECK_TYPE(obj), END, CHECK_TYPE(str), END, END */
    TEST_ASSERT_EQUAL_UINT64(6, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_BRANCH_ALLOF, code[0].op);
    TEST_ASSERT_EQUAL_UINT16(2, code[0].operand.branch.count);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[1].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[2].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[3].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[4].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[5].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 12: oneOf ────────────────────────────────────────────────────── */

void test_compiler_oneof(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *s1 = oas_schema_create(arena);
    s1->type_mask = OAS_TYPE_STRING;
    oas_schema_t *s2 = oas_schema_create(arena);
    s2->type_mask = OAS_TYPE_INTEGER;

    oas_schema_t *branches[2] = {s1, s2};
    s->one_of = branches;
    s->one_of_count = 2;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    /* BRANCH_ONEOF(2), CHECK_TYPE(str), END, CHECK_TYPE(int), END, END */
    TEST_ASSERT_EQUAL_UINT64(6, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_BRANCH_ONEOF, code[0].op);
    TEST_ASSERT_EQUAL_UINT16(2, code[0].operand.branch.count);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[1].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, code[1].type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[2].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[3].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_INTEGER, code[3].type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[4].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[5].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 13: not ──────────────────────────────────────────────────────── */

void test_compiler_not(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *inner = oas_schema_create(arena);
    inner->type_mask = OAS_TYPE_NULL;
    s->not_schema = inner;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    /* NEGATE, CHECK_TYPE(null), END, END */
    TEST_ASSERT_EQUAL_UINT64(4, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_NEGATE, code[0].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[1].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_NULL, code[1].type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[2].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[3].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 14: if/then/else ─────────────────────────────────────────────── */

void test_compiler_if_then_else(void)
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

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    size_t count = get_count(cs);
    /* COND_IF, CHECK_TYPE(str), END, COND_THEN, CHECK_MIN_LEN, END,
       COND_ELSE, CHECK_TYPE(int), END, END */
    TEST_ASSERT_EQUAL_UINT64(10, count);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_COND_IF, code[0].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[1].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[2].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_COND_THEN, code[3].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_MIN_LEN, code[4].op);
    TEST_ASSERT_EQUAL_INT64(1, code[4].operand.i64);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[5].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_COND_ELSE, code[6].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[7].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[8].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[9].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 15: enum ─────────────────────────────────────────────────────── */

void test_compiler_enum(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    /* Use a yyjson_val placeholder; the compiler just stores the pointer */
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_str(doc, arr, "a");
    yyjson_mut_arr_add_str(doc, arr, "b");
    yyjson_mut_doc_set_root(doc, arr);

    /* Convert to immutable for enum_values */
    yyjson_doc *idoc = yyjson_mut_doc_imut_copy(doc, nullptr);
    yyjson_val *root = yyjson_doc_get_root(idoc);
    TEST_ASSERT_NOT_NULL(root);

    s->enum_values = root;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    TEST_ASSERT_EQUAL_UINT64(2, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_ENUM, code[0].op);
    TEST_ASSERT_EQUAL_PTR(root, code[0].operand.ptr);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[1].op);

    oas_compiled_schema_free(cs);
    yyjson_doc_free(idoc);
    yyjson_mut_doc_free(doc);
}

/* ── Test 16: const ────────────────────────────────────────────────────── */

void test_compiler_const(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *val = yyjson_mut_str(doc, "fixed");
    yyjson_mut_doc_set_root(doc, val);

    yyjson_doc *idoc = yyjson_mut_doc_imut_copy(doc, nullptr);
    yyjson_val *root = yyjson_doc_get_root(idoc);

    s->const_value = root;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    TEST_ASSERT_EQUAL_UINT64(2, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_CONST, code[0].op);
    TEST_ASSERT_EQUAL_PTR(root, code[0].operand.ptr);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[1].op);

    oas_compiled_schema_free(cs);
    yyjson_doc_free(idoc);
    yyjson_mut_doc_free(doc);
}

/* ── Test 17: ref_resolved ─────────────────────────────────────────────── */

void test_compiler_ref_resolved(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    oas_schema_t *resolved = oas_schema_create(arena);
    resolved->type_mask = OAS_TYPE_NUMBER;
    s->ref = "#/components/schemas/Price";
    s->ref_resolved = resolved;

    oas_compiled_schema_t *cs = oas_schema_compile(s, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    /* Should compile the resolved schema, not the ref itself */
    TEST_ASSERT_EQUAL_UINT64(2, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[0].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_NUMBER, code[0].type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[1].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 18: nested 3 levels ──────────────────────────────────────────── */

void test_compiler_nested_3_levels(void)
{
    /* object -> property "tags" -> array -> items -> string */
    oas_schema_t *root = oas_schema_create(arena);
    root->type_mask = OAS_TYPE_OBJECT;

    oas_schema_t *tags = oas_schema_create(arena);
    tags->type_mask = OAS_TYPE_ARRAY;

    oas_schema_t *item = oas_schema_create(arena);
    item->type_mask = OAS_TYPE_STRING;
    tags->items = item;

    int rc = oas_schema_add_property(arena, root, "tags", tags);
    TEST_ASSERT_EQUAL_INT(0, rc);

    oas_compiled_schema_t *cs = oas_schema_compile(root, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    size_t count = get_count(cs);
    /* CHECK_TYPE(obj), ENTER_PROPERTY("tags"), CHECK_TYPE(arr),
       ENTER_ITEMS, CHECK_TYPE(str), END, END, END */
    TEST_ASSERT_EQUAL_UINT64(8, count);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[0].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_OBJECT, code[0].type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_ENTER_PROPERTY, code[1].op);
    TEST_ASSERT_EQUAL_STRING("tags", code[1].operand.str);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[2].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_ARRAY, code[2].type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_ENTER_ITEMS, code[3].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_TYPE, code[4].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_TYPE_STRING, code[4].type_mask);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[5].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[6].op);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[7].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 19: pattern with regex ───────────────────────────────────────── */

void test_compiler_pattern_with_regex(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->pattern = "^[a-z]+$";

    oas_compiler_config_t config = {.regex = regex, .format_policy = OAS_FORMAT_IGNORE};
    oas_compiled_schema_t *cs = oas_schema_compile(s, &config, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    TEST_ASSERT_EQUAL_UINT64(2, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_PATTERN, code[0].op);
    TEST_ASSERT_NOT_NULL(code[0].operand.ptr);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[1].op);

    oas_compiled_schema_free(cs);
}

/* ── Test 20: format date ──────────────────────────────────────────────── */

void test_compiler_format_date(void)
{
    oas_schema_t *s = oas_schema_create(arena);
    s->format = "date";

    oas_compiler_config_t config = {.regex = regex, .format_policy = OAS_FORMAT_ENFORCE};
    oas_compiled_schema_t *cs = oas_schema_compile(s, &config, nullptr);
    TEST_ASSERT_NOT_NULL(cs);

    const oas_instruction_t *code = get_code(cs);
    TEST_ASSERT_EQUAL_UINT64(2, get_count(cs));
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_CHECK_FORMAT, code[0].op);
    TEST_ASSERT_EQUAL_PTR((void *)(uintptr_t)oas_format_date, code[0].operand.ptr);
    TEST_ASSERT_EQUAL_UINT8(OAS_OP_END, code[1].op);

    oas_compiled_schema_free(cs);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_compiler_empty_schema);
    RUN_TEST(test_compiler_type_string);
    RUN_TEST(test_compiler_type_array_multiple);
    RUN_TEST(test_compiler_string_constraints);
    RUN_TEST(test_compiler_integer_range);
    RUN_TEST(test_compiler_multiple_of);
    RUN_TEST(test_compiler_array_items);
    RUN_TEST(test_compiler_array_prefix_items);
    RUN_TEST(test_compiler_object_required);
    RUN_TEST(test_compiler_object_properties);
    RUN_TEST(test_compiler_allof);
    RUN_TEST(test_compiler_oneof);
    RUN_TEST(test_compiler_not);
    RUN_TEST(test_compiler_if_then_else);
    RUN_TEST(test_compiler_enum);
    RUN_TEST(test_compiler_const);
    RUN_TEST(test_compiler_ref_resolved);
    RUN_TEST(test_compiler_nested_3_levels);
    RUN_TEST(test_compiler_pattern_with_regex);
    RUN_TEST(test_compiler_format_date);
    return UNITY_END();
}
