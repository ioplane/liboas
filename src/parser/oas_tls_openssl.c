#define _GNU_SOURCE

#include "oas_tls.h"

#include <errno.h>
#include <stdlib.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

struct oas_tls_ctx {
    SSL_CTX *ssl_ctx;
    SSL *ssl;
};

oas_tls_ctx_t *oas_tls_create(void)
{
    oas_tls_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return nullptr;
    }

    ctx->ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx->ssl_ctx) {
        free(ctx);
        return nullptr;
    }

    /* Load system CA certificates for verification */
    if (SSL_CTX_set_default_verify_paths(ctx->ssl_ctx) != 1) {
        SSL_CTX_free(ctx->ssl_ctx);
        free(ctx);
        return nullptr;
    }

    /* Require peer certificate verification */
    SSL_CTX_set_verify(ctx->ssl_ctx, SSL_VERIFY_PEER, nullptr);

    return ctx;
}

int oas_tls_connect(oas_tls_ctx_t *ctx, int fd, const char *hostname)
{
    if (!ctx || fd < 0 || !hostname) {
        return -EINVAL;
    }

    ctx->ssl = SSL_new(ctx->ssl_ctx);
    if (!ctx->ssl) {
        return -ENOMEM;
    }

    if (SSL_set_fd(ctx->ssl, fd) != 1) {
        SSL_free(ctx->ssl);
        ctx->ssl = nullptr;
        return -EIO;
    }

    /* Set SNI hostname for virtual hosting */
    if (SSL_set_tlsext_host_name(ctx->ssl, hostname) != 1) {
        SSL_free(ctx->ssl);
        ctx->ssl = nullptr;
        return -EIO;
    }

    /* Set hostname for certificate verification */
    SSL_set1_host(ctx->ssl, hostname);

    int ret = SSL_connect(ctx->ssl);
    if (ret != 1) {
        SSL_free(ctx->ssl);
        ctx->ssl = nullptr;
        return -EIO;
    }

    return 0;
}

int oas_tls_read(oas_tls_ctx_t *ctx, void *buf, size_t len)
{
    if (!ctx || !ctx->ssl || !buf) {
        return -EINVAL;
    }

    int n = SSL_read(ctx->ssl, buf, (int)len);
    if (n < 0) {
        int err = SSL_get_error(ctx->ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0;
        }
        return -EIO;
    }
    return n;
}

int oas_tls_write(oas_tls_ctx_t *ctx, const void *buf, size_t len)
{
    if (!ctx || !ctx->ssl || !buf) {
        return -EINVAL;
    }

    int n = SSL_write(ctx->ssl, buf, (int)len);
    if (n < 0) {
        int err = SSL_get_error(ctx->ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0;
        }
        return -EIO;
    }
    return n;
}

void oas_tls_close(oas_tls_ctx_t *ctx)
{
    if (!ctx || !ctx->ssl) {
        return;
    }

    SSL_shutdown(ctx->ssl);
    SSL_free(ctx->ssl);
    ctx->ssl = nullptr;
}

void oas_tls_destroy(oas_tls_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->ssl) {
        oas_tls_close(ctx);
    }

    if (ctx->ssl_ctx) {
        SSL_CTX_free(ctx->ssl_ctx);
    }

    free(ctx);
}
