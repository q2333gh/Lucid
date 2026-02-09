#pragma once

#include <stddef.h>
#include <stdint.h>

#include "icp_agent/error.h"
#include "icp_agent/types.h"

typedef enum {
    IC_IDENTITY_ANONYMOUS = 0,
    IC_IDENTITY_ED25519 = 1,
} ic_identity_kind_t;

typedef struct {
    ic_identity_kind_t kind;
    void              *impl;
} ic_identity_t;

ic_agent_error_code_t ic_identity_anonymous_init(ic_identity_t *out_identity);

ic_agent_error_code_t ic_identity_ed25519_generate(ic_identity_t *out_identity);

ic_agent_error_code_t ic_identity_public_key_der(const ic_identity_t *identity,
                                                 uint8_t **out_pubkey,
                                                 size_t   *out_pubkey_len);

ic_agent_error_code_t
ic_identity_sign_request_id(const ic_identity_t   *identity,
                            const ic_request_id_t *request_id,
                            uint8_t              **out_signature,
                            size_t                *out_signature_len);

void ic_identity_bytes_free(uint8_t *bytes);

void ic_identity_destroy(ic_identity_t *identity);
