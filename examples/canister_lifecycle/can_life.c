


// Example: Using pre_upgrade to save state to stable memory
//
// This example demonstrates how to use the IC_EXPORT_PRE_UPGRADE macro
// and ic_stable_save() function to persist canister state across upgrades.
//
// Usage:
//   1. Encode your state data to Candid format
//   2. Call ic_stable_save() in your pre_upgrade function
//   3. In post_upgrade, use ic_stable_restore() to retrieve the data

#include "ic_c_sdk.h"
#include <string.h>
IC_CANDID_EXPORT_DID()

#include "idl/candid.h"

// Minimal Hello World
IC_API_QUERY(greet, "() -> (text)") {
    IC_API_REPLY_TEXT("Hello from minimal C canister!");
    // ic_stable_save(NULL, 0);
}
// Example: Simple counter state on RAM 
static uint64_t counter = 0;

// Pre-upgrade function: saves state to stable memory
IC_EXPORT_PRE_UPGRADE(pre_upgrade) {
    ic_api_debug_print("canister_pre_upgrade function called");
    // Create a buffer to encode our state
    ic_buffer_t buf;
    ic_buffer_init(&buf);

    // Encode counter as Candid nat64
    // Note: In a real application, you might encode a more complex structure
    if (candid_serialize_nat(&buf, counter) != IC_OK) {
        ic_api_trap("Failed to serialize counter");
    }

    // Save to stable memory
    ic_storage_result_t result = ic_stable_save(buf.data, buf.size);
    if (result != IC_STORAGE_OK) {
        ic_buffer_free(&buf);
        ic_api_trap("Failed to save state to stable memory");
    }
    ic_api_debug_print("State saved to stable memory");
    ic_buffer_free(&buf);
}

// Post-upgrade function: restores state from stable memory
// Note: This would typically use IC_EXPORT_POST_UPGRADE macro
// (which should be implemented similarly)
void restore_state(void) {
    uint8_t *data;
    size_t   len;

    if (ic_stable_restore(&data, &len) != IC_STORAGE_OK) {
        // No saved state, use default
        counter = 0;
        return;
    }

    // Deserialize counter from Candid
    size_t offset = 0;
    if (candid_deserialize_nat(data, len, &offset, &counter) != IC_OK) {
        // Invalid data, use default
        counter = 0;
    }


}

// Example query funmction
IC_EXPORT_QUERY(get_counter) {
    ic_api_t *api = ic_api_init(IC_ENTRY_QUERY, __func__, false);
    if (!api) {
        ic_api_trap("Failed to initialize IC API");
    }

    ic_api_to_wire_nat(api, counter);
    ic_api_msg_reply(api);
    ic_api_free(api);
}

// Example update function
IC_EXPORT_UPDATE(increment) {
    ic_api_t *api = ic_api_init(IC_ENTRY_UPDATE, __func__, false);
    if (!api) {
        ic_api_trap("Failed to initialize IC API");
    }

    counter++;
    ic_api_to_wire_nat(api, counter);
    ic_api_msg_reply(api);
    ic_api_free(api);
}
