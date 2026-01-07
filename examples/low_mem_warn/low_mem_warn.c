#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

#include "idl/candid.h"
#include <stdlib.h>
#include <string.h>
#include <tinyprintf.h>

IC_API_QUERY(greet, "() -> (text)") {
    IC_API_REPLY_TEXT("Hello from minimal C canister!");
}

// --- Experimental code to consistently trigger the low-memory hook ---
// This code is tailored to the IC's Wasm memory limit/threshold mechanism. See
// comments below for details.

// Strong recommendation: To reliably observe the on_low_wasm_memory callback,
// increase the memory limit and use a smaller allocation granularity! For
// maximum effect, use parameters like: --wasm-memory-limit 512MB
// --wasm-memory-threshold 256MB Otherwise (e.g., with limit=100MB), this code
// won't trigger the callback. See:
// https://forum.dfinity.org/t/ic0539-discussion/
//
// The code allocates only 8MB per call (adjustable via MALLOC_CHUNK_MB),
// ensuring enough "distance" between threshold and limit (e.g.,
// threshold=256MB, limit=512MB).

#define MALLOC_CHUNK_MB 8
#define MALLOC_CHUNK_SIZE (MALLOC_CHUNK_MB * 1024 * 1024)
#define MALLOC_TOTAL_LIMIT_MB 512

// Hold pointers to each allocation so they aren't freed
CANISTER_STATE(void *,
               ptr_list[(MALLOC_TOTAL_LIMIT_MB / MALLOC_CHUNK_MB) + 4]) = {0};

CANISTER_STATE(size_t, allocated) = 0;

// on fish shell: run update-funciton  for 20 times
// dfx deploy && dfx canister update-settings low_mem_warn --wasm-memory-limit
// 256MB  --wasm-memory-threshold 128MB && for i in (seq 1 20); dfx canister
// call low_mem_warn request_running_memory ; end
//
//
// ========= update: each call mallocs MALLOC_CHUNK_MB MB, incrementally, up to
// the limit =========
IC_API_UPDATE(request_running_memory, "() -> (text)") {
    ic_api_debug_print(" low_mem_t1 called, start to malloc 512MB");
    char  msg[128] = {0};
    void *p = malloc(MALLOC_CHUNK_SIZE);
    if (!p) {
        ic_api_debug_print("malloc failed (no more memory)");
        IC_API_REPLY_TEXT("malloc failed (out of memory)");
        return;
    }
    // memset ensures the memory is actually allocated by the runtime
    memset(p, 0, MALLOC_CHUNK_SIZE);
    ptr_list[allocated] = p;
    allocated++;

    size_t total_alloc = allocated * MALLOC_CHUNK_MB;

    // Debug print: show total allocated, and highlight crossing threshold
    tfp_snprintf(msg, sizeof(msg), "[low_mem_t1] allocated: %zu MB",
                 total_alloc);
    ic_api_debug_print(msg);

// Log when crossing the threshold, to indicate when the system will likely
// schedule on_low_wasm_memory
#define THRESHOLD_MB 256
    if (total_alloc == THRESHOLD_MB) {
        ic_api_debug_print("---- Crossing threshold 256MB: expect "
                           "on_low_wasm_memory will be scheduled ----");
    }
#define HARDCAP_MB 512
    if (total_alloc >= HARDCAP_MB) {
        ic_api_debug_print(
            "---- Reaching 512MB, triggering trap by malloc ----");
        IC_API_REPLY_TEXT(
            "trap: hard memory limit reached (>=512MB allocated)");
        abort(); // Intentionally cause a trap when exceeding hard cap
        return;
    }

    // Normal return for client to observe progress
    tfp_snprintf(msg, sizeof(msg), "OK: allocated %zu MB, next at %zu MB",
                 total_alloc, total_alloc + MALLOC_CHUNK_MB);
    IC_API_REPLY_TEXT(msg);
}

// ========= low-memory hook handler: automatically scheduled by the IC, not a
// synchronous callback =========
IC_EXPORT_ON_LOW_WASM_MEMORY(function_on_low_wasm_memory) {
    ic_api_debug_print(
        "[on_low_wasm_memory] triggered (scheduled asynchronously by the "
        "system; replies/persistence still possible here)");
    // NOTE: This handler will NOT be called on an IC0539 "trap"—it is only
    // invoked when low memory is crossed and the canister is still operational.
    // See IC design documentation and behavior for details.
}
