// Example: Simple greet canister function
// This demonstrates basic usage of the IC C SDK

#include "greet.h"

#include <string.h>

#include "ic_c_sdk.h"

// =============================================================================
// Candid Interface Description (Auto-generated via Registry)
// =============================================================================
// Use IC_CANDID_EXPORT_DID() to export the get_candid_pointer function.
// The .did content is automatically generated from IC_QUERY/IC_UPDATE macros.
IC_CANDID_EXPORT_DID()

// =============================================================================

#include "idl/candid.h"

// =============================================================================
// Query function: greet_no_arg (no input, returns hello)
// =============================================================================
// Using IC_API_QUERY macro: simplifies API initialization and cleanup
IC_API_QUERY(greet_no_arg, "() -> (text)") {
    ic_api_debug_print("debug print: hello dfx console. ");
    IC_API_REPLY_TEXT("hello world from cdk-c !");
}

// =============================================================================
// Query function: greet_caller (no input, returns caller)
// =============================================================================
// Using IC_API_QUERY macro: simplifies API initialization and cleanup
IC_API_QUERY(greet_caller, "() -> (text)") {
    ic_principal_t caller = ic_api_get_caller(api);
    char           caller_text[64];
    ic_principal_to_text(&caller, caller_text, sizeof(caller_text));

    char msg[256] = "caller: ";
    strcat(msg, caller_text);
    ic_api_debug_print(msg);

    // Return as text
    IC_API_REPLY_TEXT(caller_text);
}

// =============================================================================
// Query function: whoami (no input, returns canister id)
// =============================================================================
IC_API_QUERY(whoami, "() -> (text)") {
    // 1. 获取自身的 Principal 结构体
    ic_principal_t self = ic_api_get_canister_self(api);

    // 2. 准备缓冲区将 Principal 转换为文本格式
    char self_text[64];
    int  len = ic_principal_to_text(&self, self_text, sizeof(self_text));

    if (len > 0) {
        char msg[128];
        strcat(msg, "My Canister ID is: ");
        strcat(msg, self_text);
        ic_api_debug_print(msg);

        // Return as text
        IC_API_REPLY_TEXT(self_text);
    } else {
        ic_api_trap("Failed to get canister id");
    }
}

// =============================================================================
// Inter-Canister Call Example
// =============================================================================

// Define callbacks
void my_reply(void *env) {
    // Handle reply
    ic_api_debug_print("Call replied!");
}

void my_reject(void *env) {
    // Handle rejection
    ic_api_debug_print("Call rejected!");
}

void make_call(ic_principal_t callee) {
    ic_call_t *call = ic_call_new(&callee, "method_name");

    // Add arguments
    uint8_t arg_data[] = {1, 2, 3};
    ic_call_with_arg(call, arg_data, sizeof(arg_data));

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

// Example update function to trigger the call (optional)
IC_API_UPDATE(trigger_call, "(principal) -> ()") {
    ic_principal_t callee;
    if (ic_api_from_wire_principal(api, &callee) != IC_OK) {
        ic_api_trap("Failed to parse callee principal");
    }

    make_call(callee);

    ic_api_msg_reply(api);
}
