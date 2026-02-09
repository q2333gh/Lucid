#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "icp_agent/error.h"

bool ic_certificate_verify_available(void);

ic_agent_error_code_t
ic_verify_certificate(const uint8_t *cert_cbor,
                      size_t         cert_cbor_len,
                      const uint8_t *root_key,
                      size_t         root_key_len,
                      const uint8_t *effective_canister_id,
                      size_t         effective_canister_id_len);
