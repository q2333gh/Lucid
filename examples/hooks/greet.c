#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

#include "idl/candid.h"

static uint64_t heartbeat_count = 0;
static uint64_t inspect_count = 0;
static uint64_t lowmem_count = 0;

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

// Helper: convert integer to string (supports only base 10, simple version)
static void int64_to_str(int64_t val, char *out, size_t out_len) {
    char buf[32];
    int  pos = 0;
    int  is_negative = 0;
    if (val < 0) {
        is_negative = 1;
        val = -val;
    }
    do {
        buf[pos++] = (char)('0' + (val % 10));
        val /= 10;
    } while (val && pos < (int)(sizeof(buf) - 1));
    if (is_negative && pos < (int)(sizeof(buf) - 1)) {
        buf[pos++] = '-';
    }
    buf[pos] = '\0';

    // Reverse and copy to out
    size_t used = 0;
    for (int i = pos - 1; i >= 0 && used + 1 < out_len; --i) {
        out[used++] = buf[i];
    }
    out[used] = '\0';
}
#include <string.h>
// Query to observe counters
IC_API_QUERY(get_hooks_counters, "() -> (text)") {
    char buf[256];
    char tmp[64];

    // Build the string step by step
    buf[0] = '\0';
    strcat(buf, "heartbeat_count=");
    int64_to_str(heartbeat_count, tmp, sizeof(tmp));
    strcat(buf, tmp);
    strcat(buf, ", inspect_count=");
    int64_to_str(inspect_count, tmp, sizeof(tmp));
    strcat(buf, tmp);
    strcat(buf, ", lowmem_count=");
    int64_to_str(lowmem_count, tmp, sizeof(tmp));
    strcat(buf, tmp);

    // Only reply with the finalized string
    IC_API_REPLY_TEXT(buf);
}
