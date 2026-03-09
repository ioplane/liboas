#include <liboas/oas_alloc.h>
#include <liboas/oas_compiler.h>
#include <liboas/oas_regex.h>
#include <liboas/oas_schema.h>
#include <liboas/oas_validator.h>

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Compile a simple schema, then validate fuzzed JSON against it */
    oas_arena_t *arena = oas_arena_create(0);
    if (!arena) {
        return 0;
    }

    oas_schema_t *schema = oas_schema_create(arena);
    if (!schema) {
        oas_arena_destroy(arena);
        return 0;
    }
    schema->type_mask = OAS_TYPE_OBJECT;

    oas_compiler_config_t config = {
        .regex = oas_regex_libregexp_create(),
        .format_policy = 0,
    };
    oas_compiled_schema_t *compiled = oas_schema_compile(schema, &config, nullptr);
    if (!compiled) {
        if (config.regex) {
            config.regex->destroy(config.regex);
        }
        oas_arena_destroy(arena);
        return 0;
    }

    oas_validation_result_t result = {0};
    (void)oas_validate_json(compiled, (const char *)data, size, &result, arena);

    oas_compiled_schema_free(compiled);
    oas_arena_destroy(arena);
    return 0;
}
