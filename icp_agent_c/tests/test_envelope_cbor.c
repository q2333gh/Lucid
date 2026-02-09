#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "icp_agent/envelope.h"

static void
assert_contains_substr(const uint8_t *buf, size_t len, const char *needle) {
    size_t needle_len = strlen(needle);
    assert(needle_len > 0);
    assert(len >= needle_len);
    for (size_t i = 0; i + needle_len <= len; i++) {
        if (memcmp(buf + i, needle, needle_len) == 0) {
            return;
        }
    }
    assert(0 && "expected substring not found");
}

static void assert_request_type_encoded(ic_envelope_type_t type,
                                        const char        *expected_type) {
    static const uint8_t  canister_id[] = {0, 0, 0, 0, 0, 0, 0x04, 0xD2};
    static const uint8_t  arg[] = {'D', 'I', 'D', 'L', 0x00, 0x00};
    static const uint8_t  sender[] = {0x04};
    ic_envelope_content_t content = {
        .type = type,
        .canister_id = canister_id,
        .canister_id_len = sizeof(canister_id),
        .method_name = "hello",
        .arg = arg,
        .arg_len = sizeof(arg),
        .sender = sender,
        .sender_len = sizeof(sender),
        .has_sender = true,
        .ingress_expiry = 1000,
        .has_ingress_expiry = true,
    };
    uint8_t *cbor = NULL;
    size_t   cbor_len = 0;
    assert(ic_encode_envelope_cbor(&content, NULL, 0, NULL, 0, &cbor,
                                   &cbor_len) == IC_AGENT_OK);
    assert(cbor != NULL);
    assert(cbor_len > 3);
    assert(cbor[0] == 0xD9 && cbor[1] == 0xD9 && cbor[2] == 0xF7);
    assert_contains_substr(cbor, cbor_len, "request_type");
    assert_contains_substr(cbor, cbor_len, expected_type);
    ic_envelope_cbor_free(cbor);
}

int main(void) {
    assert_request_type_encoded(IC_ENVELOPE_CALL, "call");
    assert_request_type_encoded(IC_ENVELOPE_QUERY, "query");
    assert_request_type_encoded(IC_ENVELOPE_READ_STATE, "read_state");
    return 0;
}
