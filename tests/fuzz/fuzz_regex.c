#include <liboas/oas_regex.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0) {
        return 0;
    }

    oas_regex_backend_t *backend = oas_regex_libregexp_create();
    if (!backend) {
        return 0;
    }

    /* Use first byte as split point between pattern and input */
    size_t split = (size_t)data[0] % size;
    if (split == 0) {
        split = 1;
    }

    char *pattern = malloc(split + 1);
    if (!pattern) {
        backend->destroy(backend);
        return 0;
    }
    memcpy(pattern, data + 1, split > 1 ? split - 1 : 0);
    pattern[split > 1 ? split - 1 : 0] = '\0';

    oas_compiled_pattern_t *compiled = nullptr;
    int rc = backend->compile(backend, pattern, &compiled);
    if (rc == 0 && compiled) {
        const uint8_t *input = data + split;
        size_t input_len = size - split;
        (void)backend->match(backend, compiled, (const char *)input, input_len);
        backend->free_pattern(backend, compiled);
    }

    free(pattern);
    backend->destroy(backend);
    return 0;
}
