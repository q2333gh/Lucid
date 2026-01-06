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

/**
 * Format HTTP response body preview (printable chars only)
 * Returns number of bytes written to buffer
 */
static size_t format_body_preview(const uint8_t *body,
                                  size_t         body_len,
                                  char          *buffer,
                                  size_t         buffer_size) {
    if (!body || body_len == 0 || !buffer || buffer_size == 0) {
        return 0;
    }

    size_t preview_len = body_len < 500 ? body_len : 500;
    size_t written = 0;

    for (size_t i = 0; i < preview_len && written < buffer_size - 1; i++) {
        if (body[i] >= 32 && body[i] < 127) {
            buffer[written++] = body[i];
        } else if (body[i] == '\n') {
            buffer[written++] = '\n';
        } else {
            buffer[written++] = '.';
        }
    }

    buffer[written] = '\0';
    return written;
}

// Reply callback - called when HTTP request succeeds
static void on_http_reply(void *env) {
    (void)env;

    ic_api_t *api = ic_api_init(IC_ENTRY_REPLY_CALLBACK, "on_http_reply", true);
    if (!api) {
        ic_api_trap("Failed to initialize API in reply callback");
        return;
    }

    ic_api_debug_print("on_http_reply: callback invoked");

    // Get the raw Candid response data from message arguments
    uint32_t response_size = ic0_msg_arg_data_size();

    char size_msg[128];
    tfp_snprintf(size_msg, sizeof(size_msg),
                 "on_http_reply: response_size = %u", response_size);
    ic_api_debug_print(size_msg);

    if (response_size == 0) {
        IC_API_REPLY_TEXT("HTTP request completed but no response data");
        ic_api_free(api);
        return;
    }

    // Allocate buffer for response data
    uint8_t *response_data = (uint8_t *)malloc(response_size);
    if (!response_data) {
        IC_API_REPLY_TEXT("Failed to allocate memory for response");
        ic_api_free(api);
        return;
    }

    // Copy response data
    ic0_msg_arg_data_copy((uintptr_t)response_data, 0, response_size);

    // Parse HTTP response
    ic_http_request_result_t result;
    ic_result_t              parse_res =
        ic_http_parse_response(response_data, response_size, &result);

    free(response_data);

    // Early exit on parse error
    if (parse_res != IC_OK) {
        char error_msg[256];
        tfp_snprintf(
            error_msg, sizeof(error_msg),
            "HTTP request completed but failed to parse response (error: %d)",
            parse_res);
        ic_api_debug_print(error_msg);
        IC_API_REPLY_TEXT(error_msg);
        ic_api_free(api);
        return;
    }

    // Parse succeeded - build response
    char debug_msg[256];
    tfp_snprintf(debug_msg, sizeof(debug_msg),
                 "on_http_reply: parse succeeded, status=%llu, "
                 "body_len=%zu, headers_count=%zu",
                 result.status, result.body_len, result.headers_count);
    ic_api_debug_print(debug_msg);

    // Build response message
    char reply[2048];
    int  offset = 0;

    offset += tfp_snprintf(reply + offset, sizeof(reply) - offset,
                           "HTTP Status: %llu\n", result.status);

    offset += tfp_snprintf(reply + offset, sizeof(reply) - offset,
                           "Body size: %zu bytes\n", result.body_len);

    // Add body preview if available
    if (result.body && result.body_len > 0) {
        offset +=
            tfp_snprintf(reply + offset, sizeof(reply) - offset, "Body: ");
        offset += format_body_preview(result.body, result.body_len,
                                      reply + offset, sizeof(reply) - offset);
    }

    IC_API_REPLY_TEXT(reply);
    ic_http_free_result(&result);

    ic_api_free(api);
}

// Reject callback - called when HTTP request fails
static void on_http_reject(void *env) {
    (void)env;

    ic_api_t *api =
        ic_api_init(IC_ENTRY_REJECT_CALLBACK, "on_http_reject", true);
    if (!api) {
        ic_api_trap("Failed to initialize API in reject callback");
        return;
    }

    ic_api_debug_print("on_http_reject: callback invoked");

    // Get reject code and message
    uint32_t reject_code = ic_api_msg_reject_code();

    char        reject_msg[512];
    size_t      reject_len = 0;
    ic_result_t msg_result =
        ic_api_msg_reject_message(reject_msg, sizeof(reject_msg), &reject_len);

    char debug_info[256];
    tfp_snprintf(debug_info, sizeof(debug_info),
                 "on_http_reject: code=%u, msg_result=%d, reject_len=%zu",
                 reject_code, msg_result, reject_len);
    ic_api_debug_print(debug_info);

    if (msg_result == IC_OK && reject_len > 0) {
        ic_api_debug_print("Reject message:");
        ic_api_debug_print(reject_msg);
    }

    char response[1024];
    tfp_snprintf(response, sizeof(response),
                 "HTTP request rejected (code=%u): %.*s", reject_code,
                 (int)reject_len, reject_msg);

    IC_API_REPLY_TEXT(response);
    ic_api_free(api);
}

/**
 * Update method to make a simple GET request
 * No transform, minimal implementation
 */
IC_API_UPDATE(http_get_simple, "() -> (text)") {
    ic_api_debug_print("http_get_simple: called");

    // Initialize HTTP request (minimal like Rust simple version)
    ic_http_request_args_t args;
    ic_http_request_args_init(&args,
                              "https://jsonplaceholder.typicode.com/todos/1");

    // Add a simple header like Rust version
    static ic_http_header_t headers[1];
    headers[0].name = "User-Agent";
    headers[0].value = "ic-http-c-demo";
    args.headers = headers;
    args.headers_count = 1;

    // Like Rust: max_response_bytes = None (use default)
    args.max_response_bytes = 0; // 0 means None
    args.transform = NULL;       // No transform for simplicity

    // Make async HTTP request
    ic_api_debug_print("http_get_simple: initiating HTTP request...");
    ic_result_t res =
        ic_http_request_async(&args, on_http_reply, on_http_reject, NULL);

    if (res != IC_OK) {
        char error_msg[256];
        tfp_snprintf(error_msg, sizeof(error_msg),
                     "Failed to initiate HTTP request: error code %d", res);
        ic_api_debug_print(error_msg);
        IC_API_REPLY_TEXT(error_msg);
        return;
    }

    ic_api_debug_print(
        "http_get_simple: HTTP request initiated, waiting for callback");

    // INFO: Do not reply here.
    // We want to reply ONLY when the callback (on_http_reply/on_http_reject)
    // returns. By returning from this function WITHOUT calling msg_reply, we
    // tell the IC that the response is pending. The callback will eventually
    // send the response.
}
