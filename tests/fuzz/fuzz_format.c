#include "compiler/oas_format.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char *str = malloc(size + 1);
    if (!str) {
        return 0;
    }
    memcpy(str, data, size);
    str[size] = '\0';

    /* Try all format validators with fuzzed input */
    static const char *formats[] = {"date", "date-time", "email",    "uri",  "uuid",
                                    "ipv4", "ipv6",      "hostname", "time", nullptr};

    for (size_t i = 0; formats[i]; i++) {
        oas_format_fn_t fn = oas_format_get(formats[i]);
        if (fn) {
            (void)fn(str, size);
        }
    }

    free(str);
    return 0;
}
