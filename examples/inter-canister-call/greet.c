// Example: Simple greet canister function
// This demonstrates basic usage of the IC C SDK

#include "greet.h"

#include <stdlib.h>
#include <string.h>

#include "ic_c_sdk.h"

#include "tinyprintf.h"

// =============================================================================
// Candid Interface Description (Auto-generated via Registry)
// =============================================================================
// Use IC_CANDID_EXPORT_DID() to export the get_candid_pointer function.
// The .did content is automatically generated from IC_QUERY/IC_UPDATE macros.
IC_CANDID_EXPORT_DID()

// =============================================================================

#include "idl/candid.h"

// =============================================================================
// Inter-Canister Call Example
// =============================================================================

// Define callbacks
void my_reply(void *env) {
    // Manually init API context for callback reply
    // Note: IC_ENTRY_REPLY_CALLBACK allows replying to the original caller
    ic_api_t *api = ic_api_init(IC_ENTRY_REPLY_CALLBACK, "my_reply", true);

    ic_api_debug_print("Call replied! Now replying to original caller.");

    // In a real app, we would decode the response from `increment` here.
    // The response is in the input buffer of the callback context.
    // For now, we just reply to the trigger_call caller with success.

    IC_API_REPLY_TEXT("Inter-canister call successful.");

    ic_api_free(api);
}

void my_reject(void *env) {
    ic_api_t *api = ic_api_init(IC_ENTRY_REJECT_CALLBACK, "my_reject", true);
    ic_api_debug_print("Call rejected!");

    // Read reject code and message from the system and propagate a helpful
    // error back to the original caller.
    uint32_t    code = ic_api_msg_reject_code();
    char        msg_buf[256];
    size_t      msg_len = 0;
    ic_result_t res =
        ic_api_msg_reject_message(msg_buf, sizeof msg_buf, &msg_len);

    char out[512];
    if (res == IC_OK) {
        tfp_snprintf(out, sizeof out,
                     "Inter-canister call rejected (code=%u, msg=\"%s\")",
                     (unsigned)code, msg_buf);
    } else {
        tfp_snprintf(out, sizeof out,
                     "Inter-canister call rejected (code=%u, msg truncated)",
                     (unsigned)code);
    }

    IC_API_REPLY_TEXT(out);

    ic_api_free(api);
}

void make_call(ic_principal_t callee, const char *method_name) {
    ic_call_t *call = ic_call_new(&callee, method_name);

    // Add cycles
    ic_call_with_cycles(call, 1000);

    // Set callbacks
    ic_call_on_reply(call, my_reply, NULL);
    ic_call_on_reject(call, my_reject, NULL);

    // Execute
    ic_call_perform(call);

    // Cleanup builder (data is already copied by system)
    ic_call_free(call);
}

// Example update function to trigger the call
// Signature: (principal, text) -> (text)
//  - first arg: target canister principal
//  - second arg: method name on that canister
IC_API_UPDATE(trigger_call, "(principal, text) -> (text)") {
    ic_api_debug_print("trigger_call called");

    // Read principal argument
    ic_principal_t callee;
    if (ic_api_from_wire_principal(api, &callee) != IC_OK) {
        ic_api_trap("Failed to parse callee principal");
    }

    // Read method name argument
    char  *method = NULL;
    size_t method_len = 0;
    if (ic_api_from_wire_text(api, &method, &method_len) != IC_OK ||
        method == NULL || method_len == 0) {
        ic_api_trap("Failed to parse method name");
    }

    char method_str[100] = {0};
    if (method_len >= sizeof(method_str)) {
        ic_api_trap("Method name string too long");
    }
    memcpy(method_str, method, method_len);
    method_str[method_len] = '\0';

    ic_api_debug_print("Method parsed: ");
    ic_api_debug_print(method_str);

    make_call(callee, method_str);

    // INFO: Do not reply here.
    // We want to reply ONLY when the callback returns.
    // By returning from this function WITHOUT calling msg_reply,
    // we tell the IC that the response is pending.
    // The callback `my_reply` will eventually send the response.
}
