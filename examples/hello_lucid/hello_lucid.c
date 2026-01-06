// Example: Simple greet canister function
// This demonstrates basic usage of the IC C SDK

#include <string.h>

#include "ic_c_sdk.h"
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

    char msg[256];
    tfp_snprintf(msg, sizeof(msg), "caller: %s", caller_text);
    ic_api_debug_print(msg);

    // Return as text
    IC_API_REPLY_TEXT(caller_text);
}

// =============================================================================
// Query function: whoami (no input, returns canister id)
// =============================================================================
IC_API_QUERY(whoami, "() -> (text)") {

    ic_principal_t self = ic_api_get_canister_self(api);

    char self_text[64];
    int  len = ic_principal_to_text(&self, self_text, sizeof(self_text));

    if (len > 0) {
        char msg[128];
        tfp_snprintf(msg, sizeof(msg), "My Canister ID is: %s", self_text);
        ic_api_debug_print(msg);

        // Return as text
        IC_API_REPLY_TEXT(self_text);
    } else {
        ic_api_trap("Failed to get canister id");
    }
}
