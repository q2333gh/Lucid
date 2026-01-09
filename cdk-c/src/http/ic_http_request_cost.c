/*
 * HTTP Request Cost Calculator
 *
 * Calculates the cycle cost for HTTP outcall requests using IC0's
 * cost_http_request system call. Computes:
 * - Request size: URL + headers + body + transform context
 * - Maximum response size (defaults to 2MB if not specified)
 * - 128-bit cycle cost returned as high/low 64-bit values
 *
 * Cycle costs must be paid upfront when making HTTP outcalls, and this
 * function provides the exact cost calculation before the call is made.
 */
#include "ic0.h"
#include "ic_http_request.h"
#include <string.h>

ic_result_t ic_http_request_cost(const ic_http_request_args_t *args,
                                 uint64_t                     *cost_high,
                                 uint64_t                     *cost_low) {
    if (!args || !args->url || !cost_high || !cost_low) {
        return IC_ERR_INVALID_ARG;
    }

    uint64_t request_size = strlen(args->url);

    for (size_t i = 0; i < args->headers_count; i++) {
        if (args->headers[i].name) {
            request_size += strlen(args->headers[i].name);
        }
        if (args->headers[i].value) {
            request_size += strlen(args->headers[i].value);
        }
    }

    if (args->body && args->body_len > 0) {
        request_size += args->body_len;
    }

    if (args->transform) {
        if (args->transform->function_method) {
            request_size += strlen(args->transform->function_method);
        }
        request_size += args->transform->context_len;
    }

    uint64_t max_res_bytes = args->max_response_bytes;
    if (max_res_bytes == 0) {
        max_res_bytes = 2000000;
    }

    uint8_t cost_bytes[16];
    ic0_cost_http_request((int64_t)request_size, (int64_t)max_res_bytes,
                          (uintptr_t)cost_bytes);

    *cost_low =
        ((uint64_t)cost_bytes[0]) | ((uint64_t)cost_bytes[1] << 8) |
        ((uint64_t)cost_bytes[2] << 16) | ((uint64_t)cost_bytes[3] << 24) |
        ((uint64_t)cost_bytes[4] << 32) | ((uint64_t)cost_bytes[5] << 40) |
        ((uint64_t)cost_bytes[6] << 48) | ((uint64_t)cost_bytes[7] << 56);

    *cost_high =
        ((uint64_t)cost_bytes[8]) | ((uint64_t)cost_bytes[9] << 8) |
        ((uint64_t)cost_bytes[10] << 16) | ((uint64_t)cost_bytes[11] << 24) |
        ((uint64_t)cost_bytes[12] << 32) | ((uint64_t)cost_bytes[13] << 40) |
        ((uint64_t)cost_bytes[14] << 48) | ((uint64_t)cost_bytes[15] << 56);

    return IC_OK;
}
