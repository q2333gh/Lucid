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
// Using IC_QUERY macro: automatically registers method AND exports function
IC_QUERY(greet_no_arg, "() -> (text)") {
    // Initialize IC API for a query function
    ic_api_t *api = ic_api_init(IC_ENTRY_QUERY, __func__, true);
    if (api == NULL) {
        ic_api_trap("Failed to initialize IC API");
    }

    // Initialize Candid arena and builder
    CandidArena arena;
    CandidArenaInit(&arena, 4096);

    CandidBuilder builder;
    CandidBuilderInit(&builder, &arena);

    // Add return argument: "hello world from cdk-c"
    const char *msg = "hello world from cdk-c";
    CandidBuilderArgText(&builder, msg, strlen(msg));

    // Serialize to bytes
    uint8_t *bytes;
    size_t   len;
    if (CandidBuilderSerialize(&builder, &bytes, &len) == CANDID_OK) {
        // Send raw response using system API
        ic0_msg_reply_data_append((uintptr_t)bytes, (uint32_t)len);
        ic0_msg_reply();
    } else {
        ic_api_trap("Failed to serialize response");
    }

    // Clean up
    CandidArenaDestroy(&arena);

    ic_api_debug_print("debug print: hello dfx console. ");

    ic_api_free(api);
}

// =============================================================================
// Query function: greet_caller (no input, returns caller)
// =============================================================================
// Using IC_QUERY macro: automatically registers method AND exports function
IC_QUERY(greet_caller, "() -> (text)") {
    // Initialize IC API for a query function
    ic_api_t *api = ic_api_init(IC_ENTRY_QUERY, __func__, true);
    if (api == NULL) {
        ic_api_trap("Failed to initialize IC API");
    }

    // Initialize Candid arena and builder
    CandidArena arena;
    CandidArenaInit(&arena, 4096);

    CandidBuilder builder;
    CandidBuilderInit(&builder, &arena);

    ic_principal_t caller = ic_api_get_caller(api);
    char           caller_text[64];
    ic_principal_to_text(&caller, caller_text, sizeof(caller_text));
    char msg[256] = "caller: ";
    strcat(msg, caller_text);
    ic_api_debug_print(msg);
    // Return as text
    CandidBuilderArgText(&builder, caller_text, strlen(caller_text));

    // Serialize to bytes
    uint8_t *bytes;
    size_t   len;
    if (CandidBuilderSerialize(&builder, &bytes, &len) == CANDID_OK) {
        ic0_msg_reply_data_append((uintptr_t)bytes, (uint32_t)len);
        ic_api_msg_reply(api);
    } else {
        ic_api_trap("Failed to serialize response");
    }

    // Clean up
    CandidArenaDestroy(&arena);
    ic_api_free(api);
}
