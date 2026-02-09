#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ic_principal.h"
#include "icp_agent/agent.h"

static int
reply_contains(const uint8_t *reply, size_t reply_len, const char *needle) {
    size_t needle_len = strlen(needle);
    if (reply == NULL || needle_len == 0 || reply_len < needle_len) {
        return 0;
    }
    for (size_t i = 0; i + needle_len <= reply_len; i++) {
        if (memcmp(reply + i, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    const char          *replica_url = getenv("ICP_AGENT_E2E_REPLICA_URL");
    const char          *canister_text = getenv("ICP_AGENT_E2E_CANISTER_ID");
    ic_principal_t       canister_principal;
    static const uint8_t didl_empty_args[] = {'D', 'I', 'D', 'L', 0x00, 0x00};
    ic_agent_t           agent;
    uint8_t             *reply = NULL;
    size_t               reply_len = 0;

    if (replica_url == NULL || canister_text == NULL) {
        fprintf(stderr,
                "[icp_agent_c_e2e_local] skipped: set "
                "ICP_AGENT_E2E_REPLICA_URL and ICP_AGENT_E2E_CANISTER_ID\n");
        return 0;
    }

    assert(ic_principal_from_text(&canister_principal, canister_text) == IC_OK);
    assert(ic_agent_init(&agent, replica_url) == IC_AGENT_OK);

    assert(ic_agent_query(&agent, canister_principal.bytes,
                          canister_principal.len, "greet", didl_empty_args,
                          sizeof(didl_empty_args), &reply,
                          &reply_len) == IC_AGENT_OK);
    assert(reply_contains(reply, reply_len, "Hello from minimal C canister!"));
    ic_agent_bytes_free(reply);
    reply = NULL;
    reply_len = 0;

    ic_agent_destroy(&agent);
    return 0;
}
