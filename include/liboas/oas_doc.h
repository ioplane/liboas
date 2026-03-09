/**
 * @file oas_doc.h
 * @brief OpenAPI 3.2 document model types.
 *
 * Top-level types for representing a parsed OpenAPI specification:
 * document, info, servers, paths, operations, parameters, responses, etc.
 */

#ifndef LIBOAS_OAS_DOC_H
#define LIBOAS_OAS_DOC_H

#include <liboas/oas_alloc.h>
#include <liboas/oas_schema.h>

#include <stdbool.h>
#include <stddef.h>

/* Forward declarations */
typedef struct oas_doc oas_doc_t;
typedef struct oas_info oas_info_t;
typedef struct oas_contact oas_contact_t;
typedef struct oas_license oas_license_t;
typedef struct oas_server oas_server_t;
typedef struct oas_server_var oas_server_var_t;
typedef struct oas_path_item oas_path_item_t;
typedef struct oas_operation oas_operation_t;
typedef struct oas_parameter oas_parameter_t;
typedef struct oas_request_body oas_request_body_t;
typedef struct oas_response oas_response_t;
typedef struct oas_media_type oas_media_type_t;
typedef struct oas_components oas_components_t;
typedef struct oas_security_req oas_security_req_t;
typedef struct oas_security_scheme oas_security_scheme_t;
typedef struct oas_tag oas_tag_t;
typedef struct oas_path_entry oas_path_entry_t;
typedef struct oas_response_entry oas_response_entry_t;
typedef struct oas_media_type_entry oas_media_type_entry_t;
typedef struct oas_schema_entry oas_schema_entry_t;
typedef struct oas_security_scheme_entry oas_security_scheme_entry_t;

struct oas_contact {
    const char *name;
    const char *url;
    const char *email;
};

struct oas_license {
    const char *name;
    const char *url;
    const char *identifier;
};

struct oas_info {
    const char *title;
    const char *summary;
    const char *description;
    const char *version;
    const char *terms_of_service;
    oas_contact_t *contact;
    oas_license_t *license;
};

struct oas_server_var {
    const char *name;
    const char *default_value;
    const char *description;
    const char **enum_values;
    size_t enum_count;
};

struct oas_server {
    const char *url;
    const char *description;
    oas_server_var_t **variables;
    size_t variables_count;
};

struct oas_media_type {
    const char *media_type_name; /**< e.g. "application/json" */
    oas_schema_t *schema;
};

struct oas_media_type_entry {
    const char *key;
    oas_media_type_t *value;
};

struct oas_parameter {
    const char *name;
    const char *in; /**< "query", "header", "path", "cookie" */
    const char *description;
    bool required;
    bool deprecated;
    oas_schema_t *schema;
};

struct oas_request_body {
    const char *description;
    bool required;
    oas_media_type_entry_t *content; /**< array of media type entries */
    size_t content_count;
};

struct oas_response {
    const char *description;
    oas_media_type_entry_t *content;
    size_t content_count;
};

struct oas_response_entry {
    const char *status_code; /**< "200", "404", "default", etc. */
    oas_response_t *response;
};

struct oas_operation {
    const char *operation_id;
    const char *summary;
    const char *description;
    const char **tags;
    size_t tags_count;
    oas_parameter_t **parameters;
    size_t parameters_count;
    oas_request_body_t *request_body;
    oas_response_entry_t *responses; /**< array of status->response entries */
    size_t responses_count;
    oas_security_req_t **security;
    size_t security_count;
    bool deprecated;
};

struct oas_path_item {
    const char *path;
    oas_operation_t *get;
    oas_operation_t *post;
    oas_operation_t *put;
    oas_operation_t *delete_;
    oas_operation_t *patch;
    oas_operation_t *head;
    oas_operation_t *options;
    oas_parameter_t **parameters;
    size_t parameters_count;
};

struct oas_path_entry {
    const char *path;
    oas_path_item_t *item;
};

struct oas_security_req {
    const char *name;
    const char **scopes;
    size_t scopes_count;
};

typedef struct {
    const char *authorization_url;
    const char *token_url;
    const char *refresh_url;
    const char **scope_names;
    const char **scope_descriptions;
    size_t scopes_count;
} oas_oauth_flow_t;

typedef struct {
    oas_oauth_flow_t *implicit;
    oas_oauth_flow_t *password;
    oas_oauth_flow_t *client_credentials;
    oas_oauth_flow_t *authorization_code;
} oas_oauth_flows_t;

struct oas_security_scheme {
    const char *type;   /**< "apiKey", "http", "oauth2", "openIdConnect" */
    const char *name;   /**< (apiKey) header/query/cookie name */
    const char *in;     /**< (apiKey) "query", "header", "cookie" */
    const char *scheme; /**< (http) "bearer", "basic", etc. */
    const char *bearer_format;
    const char *description;
    const char *open_id_connect_url; /**< (openIdConnect) discovery URL */
    oas_oauth_flows_t *flows;        /**< (oauth2) OAuth2 flow definitions */
};

struct oas_security_scheme_entry {
    const char *name;
    oas_security_scheme_t *scheme;
};

struct oas_schema_entry {
    const char *name;
    oas_schema_t *schema;
};

struct oas_tag {
    const char *name;
    const char *description;
};

typedef struct {
    const char *name;
    oas_response_t *response;
} oas_response_component_entry_t;

typedef struct {
    const char *name;
    oas_parameter_t *parameter;
} oas_parameter_component_entry_t;

typedef struct {
    const char *name;
    oas_request_body_t *request_body;
} oas_request_body_component_entry_t;

typedef struct {
    const char *name;
    oas_parameter_t *header; /**< Header Object is a subset of Parameter Object */
} oas_header_component_entry_t;

struct oas_components {
    oas_schema_entry_t *schemas;
    size_t schemas_count;
    oas_security_scheme_entry_t *security_schemes;
    size_t security_schemes_count;
    oas_response_component_entry_t *responses;
    size_t responses_count;
    oas_parameter_component_entry_t *parameters;
    size_t parameters_count;
    oas_request_body_component_entry_t *request_bodies;
    size_t request_bodies_count;
    oas_header_component_entry_t *headers;
    size_t headers_count;
};

struct oas_doc {
    const char *openapi; /**< "3.2.0" */
    oas_info_t *info;
    oas_server_t **servers;
    size_t servers_count;
    oas_path_entry_t *paths; /**< array of path entries */
    size_t paths_count;
    oas_components_t *components;
    oas_security_req_t **security;
    size_t security_count;
    oas_tag_t **tags;
    size_t tags_count;
    void *_json_doc; /**< @internal yyjson_doc* — freed by oas_doc_free() */
};

/**
 * @brief Free the underlying JSON document owned by a parsed oas_doc_t.
 *
 * Call this when you no longer need string data from the parsed document.
 * The oas_doc_t struct itself is arena-allocated and freed with the arena.
 */
void oas_doc_free(oas_doc_t *doc);

#endif /* LIBOAS_OAS_DOC_H */
