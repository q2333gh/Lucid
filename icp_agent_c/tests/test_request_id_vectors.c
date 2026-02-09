#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "icp_agent/request_id.h"

static uint8_t hex_nibble(char ch) {
    if (ch >= '0' && ch <= '9') {
        return (uint8_t)(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f') {
        return (uint8_t)(ch - 'a' + 10);
    }
    if (ch >= 'A' && ch <= 'F') {
        return (uint8_t)(ch - 'A' + 10);
    }
    return 0xFF;
}

static int decode_hex_32(const char *hex, uint8_t out[32]) {
    if (hex == NULL || strlen(hex) != 64) {
        return 0;
    }
    for (size_t i = 0; i < 32; i++) {
        uint8_t hi = hex_nibble(hex[i * 2]);
        uint8_t lo = hex_nibble(hex[i * 2 + 1]);
        if (hi > 0x0F || lo > 0x0F) {
            return 0;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 1;
}

static void assert_request_id_matches(const ic_envelope_content_t *content,
                                      const char *expected_hex) {
    ic_request_id_t out_id;
    uint8_t         expected[32];
    assert(decode_hex_32(expected_hex, expected) == 1);
    assert(ic_compute_request_id(content, &out_id) == IC_AGENT_OK);
    assert(memcmp(out_id.hash, expected, 32) == 0);
}

int main(void) {
    static const uint8_t canister_id[] = {0, 0, 0, 0, 0, 0, 0x04, 0xD2};
    static const uint8_t arg[] = {'D', 'I', 'D', 'L', 0x00, 0xFD, 0x2A};
    static const uint8_t sender_anonymous[] = {0x04};

    ic_envelope_content_t old = {
        .type = IC_ENVELOPE_CALL,
        .canister_id = canister_id,
        .canister_id_len = sizeof(canister_id),
        .method_name = "hello",
        .arg = arg,
        .arg_len = sizeof(arg),
        .sender = NULL,
        .sender_len = 0,
        .has_sender = false,
        .ingress_expiry = 0,
        .has_ingress_expiry = false,
    };

    ic_envelope_content_t current = {
        .type = IC_ENVELOPE_CALL,
        .canister_id = canister_id,
        .canister_id_len = sizeof(canister_id),
        .method_name = "hello",
        .arg = arg,
        .arg_len = sizeof(arg),
        .sender = sender_anonymous,
        .sender_len = sizeof(sender_anonymous),
        .has_sender = true,
        .ingress_expiry = 1685570400000000000ULL,
        .has_ingress_expiry = true,
    };

    assert_request_id_matches(
        &old,
        "8781291c347db32a9d8c10eb62b710fce5a93be676474c42babc74c51858f94b");
    assert_request_id_matches(
        &current,
        "1d1091364d6bb8a6c16b203ee75467d59ead468f523eb058880ae8ec80e2b101");

    return 0;
}
