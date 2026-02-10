/**
 * Example client for the `examples/adder` canister.
 *
 * This is intentionally a bit more complex than the minimal query example:
 * - Performs a `greet` query call.
 * - Performs an `increment` update call and waits for the reply.
 * - Extracts and prints the human‑readable text from both replies.
 *
 * It expects that:
 * - A local replica is running at http://127.0.0.1:4943
 * - The `adder` canister from `examples/adder` has been deployed
 *   with canister id: uxrrr-q7777-77774-qaaaq-cai
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ic_principal.h"
#include "icp_agent/agent.h"

static int extract_reply_substring(const uint8_t *reply,
                                   size_t         reply_len,
                                   const char    *needle,
                                   char          *out,
                                   size_t         out_len) {
    size_t needle_len;
    size_t i;

    if (reply == NULL || out == NULL || out_len == 0 || needle == NULL) {
        return 0;
    }

    needle_len = strlen(needle);
    if (needle_len == 0 || reply_len < needle_len) {
        return 0;
    }

    for (i = 0; i + needle_len <= reply_len; i++) {
        if (memcmp(reply + i, needle, needle_len) == 0) {
            size_t j = 0;

            while (i + j < reply_len && j + 1 < out_len) {
                unsigned char c = reply[i + j];

                /* Stop when we leave the printable ASCII range. */
                if (c < 32 || c > 126) {
                    break;
                }
                out[j] = (char)c;
                j++;
            }
            out[j] = '\0';
            return 1;
        }
    }

    return 0;
}

int main(void) {
    static const char   *REPLICA_URL = "http://127.0.0.1:4943";
    static const char   *CANISTER_TEXT = "uxrrr-q7777-77774-qaaaq-cai";
    static const uint8_t DIDL_EMPTY_ARGS[] = {'D', 'I', 'D', 'L', 0x00, 0x00};

    ic_agent_t      agent;
    ic_principal_t  canister_principal;
    ic_request_id_t req_id;
    uint8_t        *reply = NULL;
    size_t          reply_len = 0;
    int             ok;

    char greet_text[128];
    char increment_text[128];

    if (ic_principal_from_text(&canister_principal, CANISTER_TEXT) != IC_OK) {
        fprintf(stderr, "[adder_example] invalid canister principal: %s\n",
                CANISTER_TEXT);
        return 1;
    }

    if (ic_agent_init(&agent, REPLICA_URL) != IC_AGENT_OK) {
        fprintf(stderr, "[adder_example] failed to init agent for %s\n",
                REPLICA_URL);
        return 2;
    }

    /* 1) Query greet() */
    if (ic_agent_query(&agent, canister_principal.bytes, canister_principal.len,
                       "greet", DIDL_EMPTY_ARGS, sizeof(DIDL_EMPTY_ARGS),
                       &reply, &reply_len) != IC_AGENT_OK) {
        fprintf(stderr, "[adder_example] ic_agent_query(greet) failed\n");
        ic_agent_destroy(&agent);
        return 3;
    }

    ok = extract_reply_substring(reply, reply_len,
                                 "Hello from minimal C canister!", greet_text,
                                 sizeof(greet_text));
    if (ok) {
        printf("greet(): %s\n", greet_text);
    } else {
        printf("greet(): unexpected reply (len=%zu)\n", reply_len);
    }
    ic_agent_bytes_free(reply);
    reply = NULL;
    reply_len = 0;

    /* 2) Update increment() + wait for reply
     *
     * NOTE: The current icp_agent_c implementation may return a non-OK
     * error code here even when the replica has accepted the call.
     * The request_id is computed before the HTTP call, so we can still
     * proceed to wait for the reply.
     */
    {
        ic_agent_error_code_t update_rc = ic_agent_update(
            &agent, canister_principal.bytes, canister_principal.len,
            "increment", DIDL_EMPTY_ARGS, sizeof(DIDL_EMPTY_ARGS), &req_id);
        if (update_rc != IC_AGENT_OK) {
            fprintf(stderr,
                    "[adder_example] WARNING: ic_agent_update(increment) "
                    "returned %d, continuing to wait\n",
                    (int)update_rc);
        }
    }

    if (ic_agent_wait(&agent, &req_id, canister_principal.bytes,
                      canister_principal.len, &reply,
                      &reply_len) != IC_AGENT_OK) {
        fprintf(stderr, "[adder_example] ic_agent_wait(increment) failed\n");
        ic_agent_destroy(&agent);
        return 5;
    }

    ok = extract_reply_substring(reply, reply_len,
                                 "Incremented! value=", increment_text,
                                 sizeof(increment_text));
    if (ok) {
        printf("increment(): %s\n", increment_text);
    } else {
        printf("increment(): unexpected reply (len=%zu)\n", reply_len);
    }

    ic_agent_bytes_free(reply);
    reply = NULL;
    reply_len = 0;

    ic_agent_destroy(&agent);
    return 0;
}
