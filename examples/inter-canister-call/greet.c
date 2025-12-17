// Example: Simple greet canister function
// This demonstrates basic usage of the IC C SDK

#include "greet.h"

#include <stdlib.h>
#include <string.h>

#include "ic_c_sdk.h"

#include "ic_principal.h"
#include <tinyprintf.h>
#include"idl/cdk_alloc.h"

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

// Helper macro for error handling
#define TRAP_IF(cond, msg)                                                     \
    do {                                                                       \
        if (cond)                                                              \
            ic_api_trap(msg);                                                  \
    } while (0)

// Parse trigger_call text argument: "<callee>,<method_name>"
// Returns parsed callee principal and method name via output parameters.
static void parse_trigger_call_arg(ic_api_t       *api,
                                   ic_principal_t *callee_out,
                                   char           *method_out,
                                   size_t          method_buf_size) {
    char  *input = NULL;
    size_t input_len = 0;

    // Get text argument
    TRAP_IF(ic_api_from_wire_text(api, &input, &input_len) != IC_OK ||
                input == NULL || input_len == 0,
            "Failed to parse trigger_call text argument");

    // Copy to modifiable string (manual allocation for WASI compatibility)
    char *copy = (char *)malloc(input_len + 1);
    TRAP_IF(!copy, "Memory allocation failed");
    memcpy(copy, input, input_len);
    copy[input_len] = '\0';

    // Find first comma
    char *comma = strchr(copy, ',');
    TRAP_IF(!comma || comma == copy,
            "Callee string missing before comma in trigger_call argument");

    // Split string
    *comma = '\0';
    char *callee_str = copy;
    char *method_str = comma + 1;

    // Validate method name
    size_t method_len = strlen(method_str);
    TRAP_IF(method_len == 0, "Method name missing in trigger_call argument");
    TRAP_IF(method_len >= method_buf_size, "Method name string too long");

    // Parse principal
    TRAP_IF(ic_principal_from_text(callee_out, callee_str) != IC_OK,
            "Failed to parse callee principal text");

    // Copy method name
    memcpy(method_out, method_str, method_len);
    method_out[method_len] = '\0';

    free(copy);
}

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

// Example update function to trigger the call.
// Signature: (text) -> (text)
//
// The single text argument is expected to be either:
//   "<method_name>"
// or
//   "<callee>,<method_name>"
// In the second form, we split by the first comma.
// The part before the comma is the callee, the part after is the method name.
IC_API_UPDATE(trigger_call, "(text) -> (text)") {
    ic_api_debug_print("trigger_call called");

    // Parse argument: "<callee>,<method_name>"
    ic_principal_t callee;
    char           method_str[100] = {0};
    parse_trigger_call_arg(api, &callee, method_str, sizeof(method_str));

    // Debug output
    ic_api_debug_print("Parsed callee for trigger_call: ");
    {
        char callee_buf[100] = {0};
        ic_principal_to_text(&callee, callee_buf, sizeof(callee_buf));
        ic_api_debug_print(callee_buf);
    }
    ic_api_debug_print("Parsed method for trigger_call: ");
    ic_api_debug_print(method_str);

    make_call(callee, method_str);

    // INFO: Do not reply here.
    // We want to reply ONLY when the callback returns.
    // By returning from this function WITHOUT calling msg_reply,
    // we tell the IC that the response is pending.
    // The callback `my_reply` will eventually send the response.
}
