#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

#include "idl/candid.h"
#include <tinyprintf.h>

CANISTER_STATE(uint64_t, heartbeat_count) = 0;
CANISTER_STATE(uint64_t, inspect_count) = 0;
CANISTER_STATE(uint64_t, lowmem_count) = 0;

// Minimal Hello World
IC_API_QUERY(greet, "() -> (text)") {
    IC_API_REPLY_TEXT("Hello from minimal C canister!");
}

#include "idl/cdk_alloc.h"

// Heartbeat: increments a counter every round and logs
IC_EXPORT_HEARTBEAT(heartbeat) {
    heartbeat_count++;
    ic_api_debug_print("heartbeat tick");
}

// Inspect message: read-only, count inspections and log
IC_EXPORT_INSPECT_MESSAGE(inspect_message) {
    inspect_count++;
    ic_api_debug_print("inspect_message called");
}

// On low Wasm memory: attempt to log and bump counter
IC_EXPORT_ON_LOW_WASM_MEMORY(on_low_wasm_memory) {
    lowmem_count++;
    ic_api_debug_print("on_low_wasm_memory triggered");
}

// Query to observe counters
IC_API_QUERY(get_hooks_counters, "() -> (text)") {
    char buf[256];
    tfp_snprintf(buf, sizeof(buf),
                 "heartbeat_count=%llu, inspect_count=%llu, lowmem_count=%llu",
                 heartbeat_count, inspect_count, lowmem_count);

    IC_API_REPLY_TEXT(buf);
}
