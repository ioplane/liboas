#define _GNU_SOURCE

#include "oas_tls.h"

#include <errno.h>
#include <stdlib.h>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

struct oas_tls_ctx {
    WOLFSSL_CTX *wolf_ctx;
    WOLFSSL *ssl;
};

oas_tls_ctx_t *oas_tls_create(void)
{
    if (wolfSSL_Init() != WOLFSSL_SUCCESS) {
        return nullptr;
    }

    oas_tls_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        wolfSSL_Cleanup();
        return nullptr;
    }

    ctx->wolf_ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
    if (!ctx->wolf_ctx) {
        free(ctx);
        wolfSSL_Cleanup();
        return nullptr;
    }

    /* Load system CA certificates for verification */
    if (wolfSSL_CTX_load_system_CA_certs(ctx->wolf_ctx) != WOLFSSL_SUCCESS) {
        wolfSSL_CTX_free(ctx->wolf_ctx);
        free(ctx);
        wolfSSL_Cleanup();
        return nullptr;
    }

    return ctx;
}

int oas_tls_connect(oas_tls_ctx_t *ctx, int fd, const char *hostname)
{
    if (!ctx || fd < 0 || !hostname) {
        return -EINVAL;
    }

    ctx->ssl = wolfSSL_new(ctx->wolf_ctx);
    if (!ctx->ssl) {
        return -ENOMEM;
    }

    if (wolfSSL_set_fd(ctx->ssl, fd) != WOLFSSL_SUCCESS) {
        wolfSSL_free(ctx->ssl);
        ctx->ssl = nullptr;
        return -EIO;
    }

    /* Set SNI hostname for virtual hosting */
    if (wolfSSL_set_tlsext_host_name(ctx->ssl, hostname) != WOLFSSL_SUCCESS) {
        wolfSSL_free(ctx->ssl);
        ctx->ssl = nullptr;
        return -EIO;
    }

    int ret = wolfSSL_connect(ctx->ssl);
    if (ret != WOLFSSL_SUCCESS) {
        wolfSSL_free(ctx->ssl);
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

    int n = wolfSSL_read(ctx->ssl, buf, (int)len);
    if (n < 0) {
        int err = wolfSSL_get_error(ctx->ssl, n);
        if (err == WOLFSSL_ERROR_WANT_READ || err == WOLFSSL_ERROR_WANT_WRITE) {
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

    int n = wolfSSL_write(ctx->ssl, buf, (int)len);
    if (n < 0) {
        int err = wolfSSL_get_error(ctx->ssl, n);
        if (err == WOLFSSL_ERROR_WANT_READ || err == WOLFSSL_ERROR_WANT_WRITE) {
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

    wolfSSL_shutdown(ctx->ssl);
    wolfSSL_free(ctx->ssl);
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

    if (ctx->wolf_ctx) {
        wolfSSL_CTX_free(ctx->wolf_ctx);
    }

    free(ctx);
    wolfSSL_Cleanup();
}
