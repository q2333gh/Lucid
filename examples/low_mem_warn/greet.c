#include "ic_c_sdk.h"
#include <string.h>

IC_CANDID_EXPORT_DID()

#include "ic_simple.h"
#include "idl/candid.h"
#include "idl/cdk_alloc.h"

// Track how many stress writes we performed
static uint64_t      write_ticks = 0;
static ic_timer_id_t stress_timer = IC_TIMER_ID_INVALID;
static int64_t       stable_offset = 0; // append offset in stable memory

// Global timer entry point to drive timers
IC_EXPORT_GLOBAL_TIMER(global_timer) { ic_timer_process_expired(); }

// Write a large chunk to stable memory to simulate low-memory pressure
static void lowmem_write_tick(void *user_data) {
    (void)user_data;
    write_ticks++;

    // Allocate ~4MB chunk (adjust as needed for testing)
    const int64_t chunk_size = 4 * 1024 * 1024;
    uint8_t      *buf = (uint8_t *)cdk_malloc(chunk_size);
    // if (!buf) {
        // ic_api_debug_print("lowmem: malloc failed");
        // return;
    // }
    memset(buf, (int)(write_ticks & 0xFF), (size_t)chunk_size);

    // Ensure stable memory has enough space (append, not overwrite)
    int64_t       end_offset = stable_offset + chunk_size;
    const int64_t PAGE_BYTES = 64 * 1024;
    int64_t       required_pages = (end_offset + PAGE_BYTES - 1) / PAGE_BYTES;
    int64_t       current_pages = ic0_stable64_size();
    if (required_pages > current_pages) {
        char dbg_msg[128];
        int64_t add_pages = required_pages - current_pages;
        ic_api_debug_print(dbg_msg);
        if (ic0_stable64_grow(add_pages) == -1) {
            ic_api_debug_print("lowmem: stable64_grow failed");
            cdk_free(buf);
            return;
        } else {
            ic_api_debug_print("lowmem: stable64_grow success");
        }
    } else {
        ic_api_debug_print("lowmem: stable64 has enough pages, no grow needed");
    }

    // Append at current offset
    ic_stable_write(stable_offset, buf, chunk_size);
    stable_offset = end_offset;
    cdk_free(buf);

    ic_api_debug_print("lowmem: appended chunk to stable memory");
}

// Update: start periodic stress timer (interval in ms parameter)
IC_API_UPDATE(start_lowmem_stress, ("(nat64) -> (nat64)")) {
    uint64_t interval_ms = 0;
    if (ic_api_from_wire_nat(api, &interval_ms) != IC_OK) {
        ic_api_trap("invalid interval");
    }
    if (interval_ms == 0) {
        interval_ms = 500; // default 500ms
    }

    // Clear existing timer
    if (stress_timer != IC_TIMER_ID_INVALID) {
        ic_timer_clear(stress_timer);
        stress_timer = IC_TIMER_ID_INVALID;
    }

    int64_t interval_ns = (int64_t)interval_ms * 1000000LL;
    stress_timer = ic_timer_set_interval(interval_ns, lowmem_write_tick, NULL);
    if (stress_timer == IC_TIMER_ID_INVALID) {
        ic_api_trap("failed to start stress timer");
    }

    ic_api_debug_print("lowmem: stress timer started");
    IC_API_REPLY_NAT(stress_timer);
}

// Update: stop the stress timer
IC_API_UPDATE(stop_lowmem_stress, ("() -> (nat64)")) {
    if (stress_timer != IC_TIMER_ID_INVALID) {
        ic_timer_clear(stress_timer);
        stress_timer = IC_TIMER_ID_INVALID;
        ic_api_debug_print("lowmem: stress timer stopped");
    }
    IC_API_REPLY_NAT(0);
}

// Query: get current stats
IC_API_QUERY(get_lowmem_stats, "() -> (record {ticks:nat64; timer_id:nat64})") {
    ic_api_t *q = ic_api_init(IC_ENTRY_QUERY, "get_lowmem_stats", false);
    if (!q) {
        ic_api_trap("init query failed");
    }
    ic_api_to_wire_nat(q, write_ticks);
    ic_api_to_wire_nat(q, stress_timer);
    ic_api_msg_reply(q);
    ic_api_free(q);
}

// Low Wasm memory hook: log immediately when triggered
IC_EXPORT_ON_LOW_WASM_MEMORY(on_low_wasm_memory) {
  ic_api_debug_print("lowmem: on_low_wasm_memory triggered:");
    ic_api_debug_print((const char *)stable_offset);
}
