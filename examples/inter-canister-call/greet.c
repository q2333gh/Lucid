// Example: Simple greet canister function
// This demonstrates basic usage of the IC C SDK

#include "greet.h"

#include <stdlib.h>
#include <string.h>

#include "ic_c_sdk.h"

#include "ic_principal.h"
#include "idl/cdk_alloc.h"
#include <tinyprintf.h>

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

// Split a string by a delimiter character into two parts.
// Uses a static buffer; returned pointers point into this buffer.
static void split2(const char *s, char sep, char **out1, char **out2) {
    static char buf[128];
    memset(buf, 0, sizeof(buf));
    strncpy(buf, s, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    char *p = strchr(buf, sep);
    if (p) {
        *p = '\0';
        *out1 = buf;
        *out2 = p + 1;
    } else {
        *out1 = buf;
        *out2 = NULL;
    }
}

// Define callbacks
void my_reply(void *env) {
    // Manually init API context for callback reply
    // Note: IC_ENTRY_REPLY_CALLBACK allows replying to the original caller
    ic_api_t *api = ic_api_init(IC_ENTRY_REPLY_CALLBACK, "my_reply", true);

    ic_api_debug_print("Call replied! Decoding callee response.");

    char       *text = NULL;
    size_t      text_len = 0;
    ic_result_t res = ic_api_from_wire_text(api, &text, &text_len);

    if (res == IC_OK && text != NULL && text_len > 0) {
        char   out[512] = {0};
        size_t copy_len =
            text_len < sizeof(out) - 1 ? text_len : sizeof(out) - 1;
        memcpy(out, text, copy_len);
        out[copy_len] = '\0';
        ic_api_debug_print("Forwarding callee response to caller:");
        ic_api_debug_print(out);
        IC_API_REPLY_TEXT(out);
    } else {
        ic_api_debug_print(
            "Failed to decode callee response, sending fallback.");
        IC_API_REPLY_TEXT(
            "Inter-canister call succeeded but response decode failed.");
    }

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

    // Get text argument
    char  *input = NULL;
    size_t input_len = 0;

    TRAP_IF(ic_api_from_wire_text(api, &input, &input_len) != IC_OK ||
                input == NULL || input_len == 0,
            "Failed to parse trigger_call text argument");

    // Make a null-terminated copy for parsing
    char   input_nt[128];
    size_t copy_len =
        input_len < sizeof(input_nt) - 1 ? input_len : sizeof(input_nt) - 1;
    memcpy(input_nt, input, copy_len);
    input_nt[copy_len] = '\0';

    // Parse argument: "<callee>,<method_name>"
    char *callee_str = NULL;
    char *method_str = NULL;
    split2(input_nt, ',', &callee_str, &method_str);

    TRAP_IF(method_str == NULL,
            "Failed to split trigger_call argument with ','");

    // Parse principal from callee string
    ic_principal_t callee;
    TRAP_IF(ic_principal_from_text(&callee, callee_str) != IC_OK,
            "Failed to parse callee principal text");

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
