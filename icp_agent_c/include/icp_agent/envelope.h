#pragma once

#include <stddef.h>
#include <stdint.h>

#include "icp_agent/error.h"
#include "icp_agent/types.h"

ic_agent_error_code_t
ic_encode_envelope_cbor(const ic_envelope_content_t *content,
                        const uint8_t               *sender_pubkey,
                        size_t                       sender_pubkey_len,
                        const uint8_t               *sender_sig,
                        size_t                       sender_sig_len,
                        uint8_t                    **out_cbor,
                        size_t                      *out_cbor_len);

void ic_envelope_cbor_free(uint8_t *cbor_bytes);
