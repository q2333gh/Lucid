// HTTP Request API for Internet Computer
// This module provides functions for making HTTP outcalls via the management
// canister
#pragma once

#include "ic_principal.h"
#include "ic_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Type Definitions
// =============================================================================

/**
 * HTTP Method
 * Represents the HTTP method to use for the request
 */
typedef enum {
    IC_HTTP_METHOD_GET,  // GET request
    IC_HTTP_METHOD_POST, // POST request
    IC_HTTP_METHOD_HEAD  // HEAD request
} ic_http_method_t;

/**
 * HTTP Header
 * Represents a single HTTP header (name-value pair)
 */
typedef struct {
    char *name;  // Header name (e.g., "Content-Type")
    char *value; // Header value (e.g., "application/json")
} ic_http_header_t;

/**
 * Transform Context
 * Specifies a query function to transform the HTTP response for consensus
 *
 * The transform function is called with the raw HTTP response and can:
 * - Remove non-deterministic headers (e.g., Date, Set-Cookie)
 * - Normalize the response for consensus
 *
 * The function must be a query method with signature:
 *   query transform(args: TransformArgs) -> HttpRequestResult
 *
 * where TransformArgs = record { response: HttpRequestResult; context: blob }
 */
typedef struct {
    ic_principal_t
             function_principal; // Principal containing the transform function
    char    *function_method;    // Method name (must be a query function)
    uint8_t *context;            // Context blob to pass to transform function
    size_t   context_len;        // Length of context blob
} ic_transform_context_t;

/**
 * HTTP Request Arguments
 * Contains all parameters needed to make an HTTP outcall
 */
typedef struct {
    // Required fields
    char *url; // HTTP URL to call

    // Optional fields (use 0/NULL for defaults)
    uint64_t         max_response_bytes; // Max response size (0 = default 2MB)
    ic_http_method_t method;             // HTTP method (default: GET)

    // Request headers (can be NULL if headers_count is 0)
    ic_http_header_t *headers;
    size_t            headers_count;

    // Request body (can be NULL if body_len is 0)
    uint8_t *body;
    size_t   body_len;

    // Transform function (can be NULL)
    ic_transform_context_t *transform;

    // Replication mode
    // true = replicated across subnet (default, more secure)
    // false = single replica (faster, less secure)
    bool is_replicated;
} ic_http_request_args_t;

/**
 * HTTP Request Result
 * Contains the HTTP response data
 */
typedef struct {
    uint64_t status; // HTTP status code (e.g., 200, 404)

    // Response headers
    ic_http_header_t *headers;
    size_t            headers_count;

    // Response body
    uint8_t *body;
    size_t   body_len;
} ic_http_request_result_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * Calculate the cycle cost for an HTTP request
 *
 * The cost depends on:
 * - Request size (URL + headers + body + transform context)
 * - Maximum response size (max_response_bytes)
 *
 * @param args HTTP request arguments
 * @param cost_high High 64 bits of 128-bit cycle cost
 * @param cost_low Low 64 bits of 128-bit cycle cost
 * @return IC_OK on success, error code otherwise
 */
ic_result_t ic_http_request_cost(const ic_http_request_args_t *args,
                                 uint64_t                     *cost_high,
                                 uint64_t                     *cost_low);

/**
 * Make an HTTP outcall (async version)
 *
 * This function initiates an HTTP request via the management canister.
 * The call is asynchronous - the result will be delivered via callbacks.
 *
 * @param args HTTP request arguments
 * @param reply_callback Callback for successful response
 * @param reject_callback Callback for errors
 * @param user_data User data passed to callbacks
 * @return IC_OK if request was initiated, error code otherwise
 *
 * Example:
 * ```c
 * void on_reply(void *env) {
 *     // Handle response
 *     ic_http_request_result_t *result = (ic_http_request_result_t *)env;
 *     printf("Status: %llu\n", result->status);
 * }
 *
 * void on_reject(void *env) {
 *     // Handle error
 *     printf("Request failed\n");
 * }
 *
 * ic_http_request_args_t args = {
 *     .url = "https://example.com",
 *     .method = IC_HTTP_METHOD_GET,
 *     .max_response_bytes = 100000,
 *     .headers = NULL,
 *     .headers_count = 0,
 *     .body = NULL,
 *     .body_len = 0,
 *     .transform = NULL,
 *     .is_replicated = true
 * };
 *
 * ic_http_request_async(&args, on_reply, on_reject, NULL);
 * ```
 */
ic_result_t ic_http_request_async(const ic_http_request_args_t *args,
                                  void (*reply_callback)(void *env),
                                  void (*reject_callback)(void *env),
                                  void *user_data);

/**
 * Helper: Initialize HTTP request args with defaults
 *
 * @param args Arguments structure to initialize
 * @param url HTTP URL (required)
 */
static inline void ic_http_request_args_init(ic_http_request_args_t *args,
                                             const char             *url) {
    args->url = (char *)url;
    args->max_response_bytes = 0; // Use default 2MB
    args->method = IC_HTTP_METHOD_GET;
    args->headers = NULL;
    args->headers_count = 0;
    args->body = NULL;
    args->body_len = 0;
    args->transform = NULL;
    args->is_replicated = true;
}

/**
 * Parse HTTP response from Candid-encoded reply
 *
 * This function deserializes the HttpRequestResult from the management
 * canister's response. Use this in your reply callback to extract the HTTP
 * response data.
 *
 * @param candid_data Raw Candid-encoded response data
 * @param candid_len Length of Candid data
 * @param result Output: parsed HTTP response result
 * @return IC_OK on success, error code otherwise
 *
 * Example:
 * ```c
 * void on_http_reply(void *env) {
 *     ic_api_t *api = ic_api_init(IC_ENTRY_UPDATE, "on_http_reply", true);
 *
 *     // Get response data
 *     uint8_t response_data[10000];
 *     size_t response_len;
 *     ic_api_msg_arg_data(response_data, sizeof(response_data), &response_len);
 *
 *     // Parse HTTP result
 *     ic_http_request_result_t result;
 *     if (ic_http_parse_response(response_data, response_len, &result) ==
 * IC_OK) {
 *         // Use result.status, result.headers, result.body
 *         ic_http_free_result(&result);
 *     }
 *
 *     ic_api_free(api);
 * }
 * ```
 */
ic_result_t ic_http_parse_response(const uint8_t            *candid_data,
                                   size_t                    candid_len,
                                   ic_http_request_result_t *result);

/**
 * Free resources allocated in HTTP request result
 *
 * Call this function to free memory allocated by ic_http_parse_response().
 * After calling this function, the result structure should not be used.
 *
 * @param result Result structure to free
 */
void ic_http_free_result(ic_http_request_result_t *result);

/**
 * Helper: Initialize transform context
 *
 * @param ctx Transform context to initialize
 * @param principal Principal of canister containing transform function
 * @param method_name Name of the query method to call
 * @param context Context blob (can be NULL)
 * @param context_len Length of context blob
 */
static inline void
ic_http_transform_context_init(ic_transform_context_t *ctx,
                               const ic_principal_t   *principal,
                               const char             *method_name,
                               const uint8_t          *context,
                               size_t                  context_len) {
    ctx->function_principal = *principal;
    ctx->function_method = (char *)method_name;
    ctx->context = (uint8_t *)context;
    ctx->context_len = context_len;
}

#ifdef __cplusplus
}
#endif
