#pragma once

#include <stddef.h>
#include <stdint.h>

#include "icp_agent/error.h"

typedef struct {
    long     status_code;
    uint8_t *body;
    size_t   body_len;
} ic_http_response_t;

ic_agent_error_code_t ic_http_post_binary(const char         *url,
                                          const uint8_t      *body,
                                          size_t              body_len,
                                          ic_http_response_t *out_response);

void ic_http_response_free(ic_http_response_t *response);
