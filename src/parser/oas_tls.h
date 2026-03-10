/**
 * @file oas_tls.h
 * @brief TLS abstraction for HTTPS $ref fetching.
 *
 * Provides connect/read/write/close over a TLS session.
 * Compiled backend selected via -DOAS_TLS_BACKEND (wolfssl or openssl).
 */

#ifndef LIBOAS_PARSER_TLS_H
#define LIBOAS_PARSER_TLS_H

#include <stddef.h>

typedef struct oas_tls_ctx oas_tls_ctx_t;

/**
 * @brief Create TLS context (compiled backend: wolfSSL or OpenSSL).
 * @return TLS context, or nullptr if TLS not compiled in
 */
[[nodiscard]] oas_tls_ctx_t *oas_tls_create(void);

/**
 * @brief Establish TLS connection over existing socket fd.
 * @param ctx      TLS context
 * @param fd       Connected socket file descriptor
 * @param hostname Server hostname for SNI
 * @return 0 on success, negative errno on error
 */
[[nodiscard]] int oas_tls_connect(oas_tls_ctx_t *ctx, int fd, const char *hostname);

/**
 * @brief TLS read (same semantics as recv).
 * @param ctx  TLS context
 * @param buf  Buffer to read into
 * @param len  Maximum bytes to read
 * @return Bytes read, or negative errno on error
 */
int oas_tls_read(oas_tls_ctx_t *ctx, void *buf, size_t len);

/**
 * @brief TLS write (same semantics as send).
 * @param ctx  TLS context
 * @param buf  Buffer to write from
 * @param len  Bytes to write
 * @return Bytes written, or negative errno on error
 */
int oas_tls_write(oas_tls_ctx_t *ctx, const void *buf, size_t len);

/**
 * @brief Close TLS session (does not close fd).
 * @param ctx  TLS context
 */
void oas_tls_close(oas_tls_ctx_t *ctx);

/**
 * @brief Destroy TLS context and free resources.
 * @param ctx  TLS context (nullable)
 */
void oas_tls_destroy(oas_tls_ctx_t *ctx);

#endif /* LIBOAS_PARSER_TLS_H */
