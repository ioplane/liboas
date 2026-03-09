#include <liboas/oas_adapter.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const DEFAULT_TITLE = "API Documentation";

/* Scalar CDN script tag — loads latest Scalar API reference UI */
static const char *const SCALAR_CDN = "https://cdn.jsdelivr.net/npm/@scalar/api-reference";

char *oas_scalar_html(const char *title, const char *spec_url, size_t *out_len)
{
    if (!spec_url) {
        return nullptr;
    }
    if (!title) {
        title = DEFAULT_TITLE;
    }

    /* Build HTML with Scalar API Reference */
    constexpr size_t BUF_SIZE = 4096;
    char *buf = malloc(BUF_SIZE);
    if (!buf) {
        return nullptr;
    }

    int written =
        snprintf(buf, BUF_SIZE,
                 "<!DOCTYPE html>\n"
                 "<html lang=\"en\">\n"
                 "<head>\n"
                 "  <meta charset=\"utf-8\">\n"
                 "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                 "  <title>%s</title>\n"
                 "</head>\n"
                 "<body>\n"
                 "  <script id=\"api-reference\" data-url=\"%s\"></script>\n"
                 "  <script src=\"%s\"></script>\n"
                 "</body>\n"
                 "</html>\n",
                 title, spec_url, SCALAR_CDN);

    if (written < 0 || (size_t)written >= BUF_SIZE) {
        free(buf);
        return nullptr;
    }

    if (out_len) {
        *out_len = (size_t)written;
    }
    return buf;
}
