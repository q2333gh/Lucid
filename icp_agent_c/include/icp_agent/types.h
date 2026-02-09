#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t hash[32];
} ic_request_id_t;

typedef enum {
    IC_ENVELOPE_CALL = 0,
    IC_ENVELOPE_QUERY = 1,
    IC_ENVELOPE_READ_STATE = 2,
} ic_envelope_type_t;

typedef struct {
    ic_envelope_type_t type;
    const uint8_t     *canister_id;
    size_t             canister_id_len;
    const char        *method_name;
    const uint8_t     *arg;
    size_t             arg_len;
    const uint8_t     *sender;
    size_t             sender_len;
    bool               has_sender;
    uint64_t           ingress_expiry;
    bool               has_ingress_expiry;
} ic_envelope_content_t;
