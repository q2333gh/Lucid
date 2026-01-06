/**
 * HTTP Request Example - Minimal GET request
 */

#include "ic0.h"
#include "ic_c_sdk.h"
#include "ic_http_request.h"
#include <stdlib.h>
#include <string.h>
#include <tinyprintf.h>

// Export Candid DID for this canister
IC_CANDID_EXPORT_DID()

// Handler for successful HTTP response
static void handle_http_response(ic_api_t                       *api,
                                 const ic_http_request_result_t *result) {
    // Build response message
    char reply[2048];
    int  offset = 0;

    offset += tfp_snprintf(reply + offset, sizeof(reply) - offset,
                           "HTTP Status: %llu\n", result->status);

    offset += tfp_snprintf(reply + offset, sizeof(reply) - offset,
                           "Body size: %zu bytes\n", result->body_len);

    // Add body preview if available
    if (result->body && result->body_len > 0) {
        offset +=
            tfp_snprintf(reply + offset, sizeof(reply) - offset, "Body: ");
        offset +=
            ic_http_format_body_preview(result->body, result->body_len,
                                        reply + offset, sizeof(reply) - offset);
    }

    IC_API_REPLY_TEXT(reply);
}

// Handler for HTTP request rejection
static void handle_http_reject(ic_api_t                    *api,
                               const ic_http_reject_info_t *info) {
    char response[1024];
    tfp_snprintf(response, sizeof(response),
                 "HTTP request rejected (code=%u): %.*s", info->code,
                 (int)info->message_len, info->message ? info->message : "");

    IC_API_REPLY_TEXT(response);
}

/**
 * Update method to make a simple GET request
 * No transform, minimal implementation
 */
IC_API_UPDATE(http_get_simple, "() -> (text)") {
    ic_http_request_args_t args;
    ic_http_request_args_init(&args,
                              "https://jsonplaceholder.typicode.com/todos/1");

    static ic_http_header_t headers[1];
    headers[0].name = "User-Agent";
    headers[0].value = "ic-http-c-demo";
    args.headers = headers;
    args.headers_count = 1;

    args.max_response_bytes = 0; // 0 means None
    args.transform = NULL;       // No transform for simplicity

    // Make async HTTP request with simplified callbacks
    ic_result_t res = ic_http_request_async(
        &args, ic_http_reply_callback_wrapper, ic_http_reject_callback_wrapper,
        (void *)handle_http_response);

    if (res != IC_OK) {
        IC_API_REPLY_TEXT("Failed to initiate HTTP request");
        return;
    }

    // INFO: Do not reply here.
    // We want to reply ONLY when the callback (on_http_reply/on_http_reject)
    // returns. By returning from this function WITHOUT calling msg_reply, we
    // tell the IC that the response is pending. The callback will eventually
    // send the response.
}
