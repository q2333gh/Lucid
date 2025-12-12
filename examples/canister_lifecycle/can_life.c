

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
#include "idl/cdk_alloc.h"

// Minimal Hello World
IC_API_QUERY(greet, "() -> (text)") {
    IC_API_REPLY_TEXT("Hello from minimal C canister!");
    // ic_stable_save(NULL, 0);
}
// Example: Simple counter state on RAM
static uint64_t counter = 3;

// Forward declaration
void restore_state(void);

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
IC_EXPORT_POST_UPGRADE(post_upgrade) {
    ic_api_debug_print("canister_post_upgrade function called");
    restore_state();
    ic_api_debug_print("canister_post_upgrade completed");
}

// Helper function to restore state from stable memory
void restore_state(void) {
    uint8_t *data;
    size_t   len;

    if (ic_stable_restore(&data, &len) != IC_STORAGE_OK) {
        // No saved state, use default
        counter = -1;
        ic_api_debug_print("restore_state: no saved state, counter=-1");
        return;
    }

    // Deserialize counter from Candid
    size_t offset = 0;
    if (candid_deserialize_nat(data, len, &offset, &counter) != IC_OK) {
        ic_api_debug_print(
            "restore_state: Invalid Candid data, set counter to 0");
        // Invalid data, use default
        counter = -1;
    } else {
        ic_api_debug_print("restore_state() runs successfully");
    }

    // Free the buffer allocated by ic_stable_restore()
    cdk_free(data);
}

// Example query funmction
IC_API_QUERY(get_counter, "() -> (nat64)") { IC_API_REPLY_NAT(counter); }
int int_to_str(int value, char *str) {
    char buf[12]; // Enough to hold -2147483648 and '\0'
    int  i = 0;
    int  is_negative = 0;

    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return 1;
    }

    if (value < 0) {
        is_negative = 1;
        // Handle INT_MIN specially
        if (value == INT32_MIN) {
            value += 1; // Add 1 first to avoid overflow
            value = -value;
        } else {
            value = -value;
        }
    }

    // Generate reversed digit string
    while (value > 0) {
        buf[i++] = (value % 10) + '0';
        value /= 10;
    }

    // If INT_MIN, fix first digit as 8
    if (is_negative && i > 0 && buf[0] == '8')
        buf[0] = '8';

    if (is_negative)
        buf[i++] = '-';

    // Reverse into output string
    int j;
    for (j = 0; j < i; ++j) {
        str[j] = buf[i - j - 1];
    }
    str[i] = '\0';

    return i;
}
// Read-only query that triggers restore_state once and prints debug info
IC_API_QUERY(restore_and_debug, "() -> (text)") {
    ic_api_debug_print("restore_and_debug: start");
    restore_state();

    char msg[96] = "restore_and_debug: counter=";
    char numbuf[32];
    int_to_str((unsigned long long)counter, numbuf);
    strcat(msg, numbuf);

    ic_api_debug_print(msg);

    IC_API_REPLY_TEXT(msg);
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
