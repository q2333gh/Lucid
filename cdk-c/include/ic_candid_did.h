// Candid Interface Description (.did) embedding support
// Provides macros to embed candid interface in WASM for extraction by dfx/candid-extractor
#pragma once

// =============================================================================
// IC_CANDID_INTERFACE Macro
// =============================================================================
// Use this macro ONCE in your canister code to define the candid interface.
// The interface string will be embedded in WASM and can be extracted by:
//   - dfx (automatically during deployment)
//   - candid-extractor tool
//
// Example usage:
//   IC_CANDID_INTERFACE("service : { greet : (text) -> (text) query; }");
//
// The macro generates:
//   1. A static string containing the candid interface
//   2. An exported function exclusive func name called: `get_candid_pointer` that returns the string address
// =============================================================================

#define IC_CANDID_INTERFACE(did_string)                                                            \
    static const char *__ic_candid_interface_str = did_string;                                     \
                                                                                                   \
    __attribute__((export_name("get_candid_pointer"))) __attribute__((visibility("default")))      \
    const char *                                                                                   \
    get_candid_pointer(void) {                                                                     \
        return __ic_candid_interface_str;                                                          \
    }
