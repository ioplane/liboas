#include <liboas/oas_doc.h>

#include <yyjson.h>

void oas_doc_free(oas_doc_t *doc)
{
    if (!doc) {
        return;
    }
    if (doc->_json_doc) {
        yyjson_doc_free((yyjson_doc *)doc->_json_doc);
        doc->_json_doc = nullptr;
    }
}
