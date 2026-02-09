#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "icp_agent/certificate.h"

static const uint8_t CERT_MAP_OK[] = {0xA1, 0x6B, 0x63, 0x65, 0x72, 0x74,
                                      0x69, 0x66, 0x69, 0x63, 0x61, 0x74,
                                      0x65, 0x42, 0x01, 0x02};
static const uint8_t CERT_MAP_BAD[] = {0xA1, 0x63, 0x66, 0x6F, 0x6F, 0x01};
static const uint8_t ROOT_KEY[] = {1, 2, 3, 4};
static const uint8_t CANISTER_ID[] = {0, 0, 0, 0, 0, 0, 0x04, 0xD2};

int main(void) {
    ic_agent_error_code_t rc;

    rc = ic_verify_certificate(NULL, 0, ROOT_KEY, sizeof(ROOT_KEY), CANISTER_ID,
                               sizeof(CANISTER_ID));
    assert(rc == IC_AGENT_ERR);

    rc = ic_verify_certificate(CERT_MAP_BAD, sizeof(CERT_MAP_BAD), ROOT_KEY,
                               sizeof(ROOT_KEY), CANISTER_ID,
                               sizeof(CANISTER_ID));
    assert(rc == IC_AGENT_ERR_INVALID_CBOR);

    rc = ic_verify_certificate(CERT_MAP_OK, sizeof(CERT_MAP_OK), ROOT_KEY,
                               sizeof(ROOT_KEY), CANISTER_ID,
                               sizeof(CANISTER_ID));
    if (ic_certificate_verify_available()) {
        assert(rc == IC_AGENT_ERR_VERIFY_FAILED || rc == IC_AGENT_OK);
    } else {
        assert(rc == IC_AGENT_ERR_UNSUPPORTED);
    }

    return 0;
}
