# Sprint 10: Codex Review Bugfixes

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Fix 3 bugs found by Codex on PR #1 — compile failure propagation, IPv6 validation, regex backend ownership.

**Architecture:** All fixes are localized to `src/compiler/` — no new modules, no API changes. Bug 1 changes `compile_media_types()` and `compile_params()` return semantics. Bug 2 adds a guard in `oas_format_ipv6()`. Bug 3 adds `bool owns_regex` to `oas_compiled_doc`.

**Tech Stack:** C23, yyjson, Unity 2.6.1, libregexp (regex tests)

**Skills:** `@modern-c23`, `@liboas-architecture`

**Build/test:**
```bash
podman run --rm --security-opt seccomp=unconfined \
  --env-file .env -v $(pwd):/workspace:Z \
  localhost/liboas-dev:latest bash -c "cd /workspace && \
  cmake --preset clang-debug && cmake --build --preset clang-debug && \
  ctest --preset clang-debug --output-on-failure"
```

---

## Task 10.1: IPv6 Format Validator — Reject Bare IPv4

**Files:**
- Modify: `src/compiler/oas_format.c:361-367`
- Modify: `tests/unit/test_format.c`

**Bug:** When `.` is found during IPv6 parsing, code walks back to find IPv4 start but never checks that any colon-separated groups preceded it. `"1.2.3.4"` passes as valid IPv6.

**Step 1: Write failing tests**

Add to `tests/unit/test_format.c` after `test_format_ipv6_valid`:

```c
void test_format_ipv6_rejects_bare_ipv4(void)
{
    /* Pure IPv4 addresses must NOT be accepted as IPv6 */
    TEST_ASSERT_FALSE(oas_format_ipv6("1.2.3.4", 7));
    TEST_ASSERT_FALSE(oas_format_ipv6("192.168.1.1", 11));
    TEST_ASSERT_FALSE(oas_format_ipv6("255.255.255.255", 15));
    TEST_ASSERT_FALSE(oas_format_ipv6("0.0.0.0", 7));
}

void test_format_ipv6_accepts_mapped_ipv4(void)
{
    /* Valid IPv4-mapped IPv6 addresses */
    const char *mapped = "::ffff:192.168.1.1";
    TEST_ASSERT_TRUE(oas_format_ipv6(mapped, strlen(mapped)));
    const char *compat = "::192.168.1.1";
    TEST_ASSERT_TRUE(oas_format_ipv6(compat, strlen(compat)));
    const char *full = "2001:db8::192.168.1.1";
    TEST_ASSERT_TRUE(oas_format_ipv6(full, strlen(full)));
    const char *full6 = "64:ff9b::192.0.2.1";
    TEST_ASSERT_TRUE(oas_format_ipv6(full6, strlen(full6)));
}
```

Register in `main()`:
```c
RUN_TEST(test_format_ipv6_rejects_bare_ipv4);
RUN_TEST(test_format_ipv6_accepts_mapped_ipv4);
```

**Step 2: Run tests — expect FAIL**

```bash
ctest --preset clang-debug -R test_format --output-on-failure
```
Expected: `test_format_ipv6_rejects_bare_ipv4` FAIL (bare IPv4 incorrectly accepted)

**Step 3: Fix `oas_format_ipv6()`**

In `src/compiler/oas_format.c`, replace the IPv4-mapped suffix block (lines 361-367):

```c
        } else if (value[i] == '.') {
            /* IPv4-mapped suffix — must have at least one colon before the IPv4 part */
            size_t v4_start = i;
            while (v4_start > 0 && value[v4_start - 1] != ':') {
                v4_start--;
            }
            /* Bare IPv4 (no colon prefix) is not valid IPv6 */
            if (v4_start == 0) {
                return false;
            }
            return oas_format_ipv4(value + v4_start, len - v4_start);
```

**Step 4: Run tests — expect PASS**

```bash
ctest --preset clang-debug -R test_format --output-on-failure
```
Expected: all PASS (21 tests)

**Step 5: Commit**

```bash
git add src/compiler/oas_format.c tests/unit/test_format.c
git commit -m "fix: IPv6 format validator rejects bare IPv4 addresses (2 tests)"
```

---

## Task 10.2: Compile Failure Propagation in Doc Compiler

**Files:**
- Modify: `src/compiler/oas_doc_compiler.c` — `compile_media_types()`, `compile_params()`
- Create: `tests/unit/test_doc_compiler.c` — dedicated doc compiler tests
- Modify: `CMakeLists.txt` — register new test

**Bug:** `compile_media_types()` and `compile_params()` silently `continue` when `oas_schema_compile()` returns NULL. The function reports success even when schemas fail. Downstream code assumes all schemas compiled.

**Step 1: Write failing tests**

Create `tests/unit/test_doc_compiler.c`:

```c
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
        oas_request_body_t *rb =
            oas_arena_alloc(arena, sizeof(*rb), _Alignof(oas_request_body_t));
        memset(rb, 0, sizeof(*rb));
        rb->required = true;

        oas_media_type_t *mt =
            oas_arena_alloc(arena, sizeof(*mt), _Alignof(oas_media_type_t));
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

    oas_response_entry_t *re =
        oas_arena_alloc(arena, sizeof(*re), _Alignof(oas_response_entry_t));
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

/* ── Test: valid schema compiles successfully ────────────────────────── */

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

/* ── Test: unresolved $ref propagates failure ───────────────────────── */

void test_doc_compile_unresolved_ref_fails(void)
{
    /* A schema with $ref but no ref_resolved should fail to compile */
    oas_schema_t *s = oas_schema_create(arena);
    s->ref = "#/components/schemas/Missing";
    /* ref_resolved left as nullptr — compiler should treat this as failure */

    oas_doc_t *doc = build_doc_with_schema(s);
    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, nullptr, errors);
    /* After fix: should return nullptr because compile_media_types detected failure */
    TEST_ASSERT_NULL(cdoc);
}

/* ── Test: compile accumulates errors from multiple schemas ─────────── */

void test_doc_compile_accumulates_errors(void)
{
    /* Build doc with two content types, both with unresolved refs */
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

    /* Two media types with unresolved refs */
    oas_media_type_entry_t *mtes =
        oas_arena_alloc(arena, 2 * sizeof(*mtes), _Alignof(oas_media_type_entry_t));

    oas_schema_t *s1 = oas_schema_create(arena);
    s1->ref = "#/components/schemas/A";

    oas_schema_t *s2 = oas_schema_create(arena);
    s2->ref = "#/components/schemas/B";

    oas_media_type_t *mt1 = oas_arena_alloc(arena, sizeof(*mt1), _Alignof(oas_media_type_t));
    mt1->media_type_name = "application/json";
    mt1->schema = s1;
    mtes[0].key = "application/json";
    mtes[0].value = mt1;

    oas_media_type_t *mt2 = oas_arena_alloc(arena, sizeof(*mt2), _Alignof(oas_media_type_t));
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
    oas_response_entry_t *re =
        oas_arena_alloc(arena, sizeof(*re), _Alignof(oas_response_entry_t));
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
    TEST_ASSERT_NULL(cdoc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_doc_compile_valid_schema_succeeds);
    RUN_TEST(test_doc_compile_unresolved_ref_fails);
    RUN_TEST(test_doc_compile_accumulates_errors);
    return UNITY_END();
}
```

**Step 2: Register test in CMakeLists.txt**

Add after the `test_request` line:
```cmake
oas_add_test(test_doc_compiler tests/unit/test_doc_compiler.c oas_validator oas_parser)
```

**Step 3: Run tests — expect FAIL**

```bash
ctest --preset clang-debug -R test_doc_compiler --output-on-failure
```
Expected: `test_doc_compile_unresolved_ref_fails` and `test_doc_compile_accumulates_errors` FAIL (currently returns non-NULL because failures are silently skipped)

**Step 4: Fix `compile_media_types()`**

In `src/compiler/oas_doc_compiler.c`, modify `compile_media_types()`:

```c
static int compile_media_types(oas_compiled_doc_t *cdoc, const oas_media_type_entry_t *entries,
                               size_t count, const oas_compiler_config_t *config,
                               oas_error_list_t *errors, compiled_media_type_t **out,
                               size_t *out_count)
{
    if (count == 0 || !entries) {
        *out = nullptr;
        *out_count = 0;
        return 0;
    }

    compiled_media_type_t *arr =
        oas_arena_alloc(cdoc->arena, count * sizeof(*arr), _Alignof(compiled_media_type_t));
    if (!arr) {
        return -ENOMEM;
    }

    size_t compiled = 0;
    int ret = 0;
    for (size_t i = 0; i < count; i++) {
        if (!entries[i].value || !entries[i].value->schema) {
            continue;
        }

        oas_compiled_schema_t *cs = oas_schema_compile(entries[i].value->schema, config, errors);
        if (!cs) {
            ret = -EINVAL;
            continue; /* accumulate errors from remaining schemas */
        }

        int rc = track_schema(cdoc, cs);
        if (rc < 0) {
            oas_compiled_schema_free(cs);
            return rc;
        }

        arr[compiled].content_type = entries[i].key;
        arr[compiled].schema = cs;
        compiled++;
    }

    *out = arr;
    *out_count = compiled;
    return ret;
}
```

Apply the same pattern to `compile_params()`:

```c
    size_t compiled = 0;
    int ret = 0;
    for (size_t i = 0; i < count; i++) {
        if (!parameters[i] || !parameters[i]->schema) {
            continue;
        }

        oas_compiled_schema_t *cs = oas_schema_compile(parameters[i]->schema, config, errors);
        if (!cs) {
            ret = -EINVAL;
            continue;
        }
        // ... rest unchanged
    }

    *out = arr;
    *out_count = compiled;
    return ret;
```

Then propagate error from `compile_media_types` → `compile_operation` → `oas_doc_compile`:

In `compile_operation()`, check return of `compile_media_types()` and `compile_params()`:
```c
    /* Track any compilation failures */
    int compile_err = 0;

    if (op->request_body) {
        out->request_body_required = op->request_body->required;
        int rc = compile_media_types(cdoc, op->request_body->content,
                                     op->request_body->content_count, config, errors,
                                     &out->request_body, &out->request_body_count);
        if (rc == -ENOMEM) {
            return rc;
        }
        if (rc < 0) {
            compile_err = rc;
        }
    } else {
        out->request_body = nullptr;
        out->request_body_count = 0;
    }

    int rc = compile_responses(cdoc, op->responses, op->responses_count, config, errors,
                               &out->responses, &out->responses_count);
    if (rc == -ENOMEM) {
        return rc;
    }
    if (rc < 0) {
        compile_err = rc;
    }

    rc = compile_params(cdoc, op->parameters, op->parameters_count, config, errors, &out->params,
                        &out->params_count);
    if (rc == -ENOMEM) {
        return rc;
    }
    if (rc < 0) {
        compile_err = rc;
    }

    return compile_err;
```

In the main `oas_doc_compile()` loop, track failure:
```c
    int compile_err = 0;
    for (size_t i = 0; i < doc->paths_count; i++) {
        // ... existing method iteration ...
        for (size_t m = 0; m < sizeof(methods) / sizeof(methods[0]); m++) {
            if (!methods[m].op) {
                continue;
            }
            int rc = compile_operation(cdoc, path, methods[m].method, methods[m].op, config, errors,
                                       &cdoc->operations[op_idx]);
            if (rc == -ENOMEM) {
                goto fail;
            }
            if (rc < 0) {
                compile_err = rc;
            }
            op_idx++;
        }
    }
    cdoc->operations_count = op_idx;

    if (compile_err < 0) {
        goto fail;
    }
```

Also apply same pattern in `compile_responses()`:
```c
        int rc = compile_media_types(cdoc, entries[i].response->content,
                                     entries[i].response->content_count, config, errors,
                                     &arr[compiled].content, &arr[compiled].content_count);
        if (rc == -ENOMEM) {
            return rc;
        }
        if (rc < 0) {
            ret = rc;
        }
        compiled++;
```

**Step 5: Run tests — expect PASS**

```bash
ctest --preset clang-debug --output-on-failure
```
Expected: ALL tests PASS (existing + 3 new). The `test_doc_compile_unresolved_ref_fails` and `test_doc_compile_accumulates_errors` now correctly get NULL.

**Important:** Verify existing tests `test_request` and `test_petstore` still pass — their schemas are valid so `compile_media_types` should return 0.

**Step 6: Commit**

```bash
git add src/compiler/oas_doc_compiler.c tests/unit/test_doc_compiler.c CMakeLists.txt
git commit -m "fix: propagate schema compilation failures through doc compiler (3 tests)"
```

---

## Task 10.3: Regex Backend Ownership — Prevent Use-After-Free

**Files:**
- Modify: `src/compiler/oas_doc_compiler.c` — add `owns_regex` flag
- Modify: `tests/unit/test_doc_compiler.c` — add ownership tests
- Modify: `include/liboas/oas_compiler.h` — add `bool owns_regex` to config

**Bug:** `oas_doc_compile()` stores `config->regex` directly, then `oas_compiled_doc_free()` destroys it. If caller reuses the backend across multiple compilations, use-after-free occurs.

**Step 1: Write failing tests**

Add to `tests/unit/test_doc_compiler.c`:

```c
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

    /* Freeing cdoc should also free regex (default ownership) */
    oas_compiled_doc_free(cdoc);
    /* No crash = success. regex was freed by oas_compiled_doc_free. */
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
        .owns_regex = false, /* caller retains ownership */
    };
    oas_compiled_doc_t *cdoc = oas_doc_compile(doc, &config, errors);
    TEST_ASSERT_NOT_NULL(cdoc);

    oas_compiled_doc_free(cdoc);

    /* regex should still be alive — caller owns it */
    /* Compile a second doc with same backend to prove no use-after-free */
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

    /* Each gets its own regex backend — default ownership */
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
```

Register in `main()`:
```c
RUN_TEST(test_doc_compile_owns_regex_by_default);
RUN_TEST(test_doc_compile_borrowed_regex_not_freed);
RUN_TEST(test_doc_compile_two_docs_separate_regex);
```

**Step 2: Add `owns_regex` to config**

In `include/liboas/oas_compiler.h`, modify `oas_compiler_config_t`:

```c
typedef struct {
    oas_regex_backend_t *regex;
    uint8_t format_policy; /**< oas_format_policy_t */
    bool owns_regex;       /**< If true (default), compiled doc takes ownership and frees regex. */
} oas_compiler_config_t;
```

**Step 3: Modify `oas_doc_compile()`**

In `src/compiler/oas_doc_compiler.c`, change how `cdoc->regex` is set. Add `bool owns_regex` to `struct oas_compiled_doc`:

```c
struct oas_compiled_doc {
    oas_path_matcher_t *matcher;
    compiled_operation_t *operations;
    size_t operations_count;
    oas_compiled_schema_t **all_schemas;
    size_t all_schemas_count;
    size_t all_schemas_capacity;
    oas_regex_backend_t *regex;
    bool owns_regex; /**< If true, regex freed on destroy */
    oas_arena_t *arena;
};
```

In `oas_doc_compile()`, set ownership:
```c
    if (config && config->regex) {
        cdoc->regex = config->regex;
        cdoc->owns_regex = !config || config->owns_regex || !config->regex;
    }
```

Wait — the default should be "owned" when `owns_regex` is not explicitly set. Since C zero-initializes bools to false, we need to flip the semantics. Better approach:

```c
    if (config && config->regex) {
        cdoc->regex = config->regex;
        /* Default: take ownership unless caller explicitly opts out */
        cdoc->owns_regex = !config->owns_regex ? true : false;
    }
```

Hmm, that's confusing. Cleaner: rename to `borrow_regex` (false by default = takes ownership):

Actually simplest: keep `owns_regex` in the config but make it `true = takes ownership`. Since zero-init = false, callers who don't set it get `false` meaning "no ownership taken" — **but that breaks existing code**.

Best approach: The struct field is called `owns_regex` on `oas_compiled_doc`. We determine it from config:

```c
    if (config && config->regex) {
        cdoc->regex = config->regex;
    }
    /* Ownership: default is to take ownership (backward compat).
     * If caller sets owns_regex = false, we don't free on destroy. */
    cdoc->owns_regex = !(config && !config->owns_regex && config->regex);
```

This is getting complex. Simplest correct approach — new field name in config to avoid zero-init confusion:

In `oas_compiler_config_t`:
```c
    bool borrow_regex; /**< If true, caller retains regex ownership (default: false = doc takes ownership) */
```

In `oas_doc_compile()`:
```c
    if (config && config->regex) {
        cdoc->regex = config->regex;
        cdoc->owns_regex = !config->borrow_regex;
    }
```

In `oas_compiled_doc_free()`:
```c
    if (compiled->regex && compiled->owns_regex) {
        compiled->regex->destroy(compiled->regex);
    }
```

Zero-init `borrow_regex = false` → takes ownership → backward compatible.

**Step 4: Run tests — expect PASS**

```bash
ctest --preset clang-debug --output-on-failure
```
Expected: ALL tests PASS. Key verification: `test_doc_compile_borrowed_regex_not_freed` creates two compiled docs with same regex backend without crashing.

**Step 5: Commit**

```bash
git add include/liboas/oas_compiler.h src/compiler/oas_doc_compiler.c tests/unit/test_doc_compiler.c
git commit -m "fix: regex backend ownership — borrow_regex prevents use-after-free (3 tests)"
```

---

## Task 10.4: Quality Pipeline

**Step 1: Run full quality pipeline**

```bash
podman run --rm --security-opt seccomp=unconfined \
  --env-file .env -v $(pwd):/workspace:Z \
  localhost/liboas-dev:latest bash -c "cd /workspace && ./scripts/quality.sh"
```

**Step 2: Fix any findings**

Expected issues: clang-format may need reformatting on new test code. Run format, fix any cppcheck/PVS warnings.

**Step 3: Commit fixes if any**

```bash
git commit -m "chore: quality pipeline clean — Sprint 10"
```

---

## Summary

| Task | Tests | Files Modified |
|------|-------|---------------|
| 10.1 IPv6 bare IPv4 | 2 | `oas_format.c`, `test_format.c` |
| 10.2 Compile failure propagation | 3 | `oas_doc_compiler.c`, `test_doc_compiler.c`, `CMakeLists.txt` |
| 10.3 Regex ownership | 3 | `oas_doc_compiler.c`, `oas_compiler.h`, `test_doc_compiler.c` |
| 10.4 Quality pipeline | 0 | formatting/lint fixes |
| **Total** | **8** | |
