/**
 * @file oas_negotiate.h
 * @brief HTTP content negotiation utilities.
 */

#ifndef LIBOAS_OAS_NEGOTIATE_H
#define LIBOAS_OAS_NEGOTIATE_H

#include <stddef.h>

/**
 * @brief Select best matching media type from an Accept header.
 * @param accept    Accept header value (e.g., "application/json, text/html;q=0.9").
 * @param available Array of available media types from OAS operation.
 * @param count     Number of available types.
 * @return Best match from @p available, or nullptr if no acceptable type.
 */
[[nodiscard]] const char *oas_negotiate_content_type(const char *accept, const char **available,
                                                     size_t count);

#endif /* LIBOAS_OAS_NEGOTIATE_H */
