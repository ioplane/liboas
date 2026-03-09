#include "core/oas_jsonptr.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

static const char SAMPLE_DOC[] = "{\"a\":{\"b\":[1,2,{\"c\":\"deep\"}]},\"d\":\"value\"}";

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Fuzz the JSON Pointer resolver with random pointers */
    char *ptr = malloc(size + 1);
    if (!ptr) {
        return 0;
    }
    memcpy(ptr, data, size);
    ptr[size] = '\0';

    yyjson_doc *jdoc = yyjson_read(SAMPLE_DOC, sizeof(SAMPLE_DOC) - 1, 0);
    if (!jdoc) {
        free(ptr);
        return 0;
    }

    yyjson_val *root = yyjson_doc_get_root(jdoc);
    (void)oas_jsonptr_resolve(root, ptr);

    yyjson_doc_free(jdoc);
    free(ptr);
    return 0;
}
