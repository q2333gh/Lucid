#pragma once

typedef enum {
    IC_AGENT_OK = 0,
    IC_AGENT_ERR = 1,
    IC_AGENT_ERR_UNSUPPORTED = 2,
    IC_AGENT_ERR_INVALID_CBOR = 3,
    IC_AGENT_ERR_VERIFY_FAILED = 4,
} ic_agent_error_code_t;
