#include "ic_c_sdk.h"
#include <string.h>
#include "ic_simple.h"
#include "tinyprintf.h"
#include"idl/cdk_alloc.h"
IC_CANDID_EXPORT_DID()

// Example usage:
// dfx deploy && dfx canister update-settings low_mem_warn --wasm-memory-limit 300MB --wasm-memory-threshold 200MB && dfx canister call low_mem_warn start_lowmem_stress

// Loop: malloc memory and write to stable memory until it fails or 400MB is written.
IC_API_UPDATE(start_lowmem_stress, ("() -> (nat64)")) {
    ic_api_debug_print(" start_lowmem_stress called (write only 400MB)");
    int32_t write_ticks = 0;
    int64_t stable_offset = 0;
    const int64_t chunk_size = 262144; // 256KB
    const int64_t max_bytes = 400LL * 1024 * 1024; // 400MB limit

    while (stable_offset < max_bytes) {
        uint8_t *buf = (uint8_t *)malloc(chunk_size);
        if (!buf) {
            char msg[100];
            tfp_snprintf(msg, sizeof(msg), "lowmem: malloc failed after %ld ticks, stable_offset=%ld, pause", (long)write_ticks, (long)stable_offset);
            ic_api_debug_print(msg);
            break;
        }
        memset(buf, (int)(write_ticks & 0xFF), (size_t)chunk_size); // fill buffer

        int64_t end_offset = stable_offset + chunk_size;
        if (end_offset > max_bytes) {
            end_offset = max_bytes;
        }
        int64_t this_chunk = end_offset - stable_offset;

        const int64_t PAGE_BYTES = 64 * 1024; // 64KB per stable memory page
        int64_t required_pages = (end_offset + PAGE_BYTES - 1) / PAGE_BYTES;
        int64_t current_pages = ic0_stable64_size();
        if (required_pages > current_pages) {
            int64_t add_pages = required_pages - current_pages;
            int grow_ret = ic0_stable64_grow(add_pages);
            if (grow_ret == -1) {
                char msg[120];
                // If grow fails, log (optional) and stop. Do not free buf to simulate memory pressure.
                break;
            } else {
                char msg[120];
                tfp_snprintf(msg, sizeof(msg), "stable64_grow succeed: grew %ld pages, total pages now %ld", (long)add_pages, (long)ic0_stable64_size());
                ic_api_debug_print(msg);
            }
        }

        ic_stable_write(stable_offset, buf, this_chunk);
        {
            char msg[100];
            tfp_snprintf(msg, sizeof(msg), "wrote %ld bytes at offset %ld, tick %lu", (long)this_chunk, (long)stable_offset, (long)write_ticks);
            // Logging can be enabled here
        }
        stable_offset = end_offset;
        write_ticks++;

        // Do not free buf (intentionally leak memory to test low-mem conditions)
    }

    char msg[80];
    tfp_snprintf(msg, sizeof(msg), "finished after %ld ticks, total stable_offset=%ld", (long)write_ticks, (long)stable_offset);
    // Optional: log msg

    IC_API_REPLY_NAT(write_ticks);
}

// Example usage:
// dfx deploy && dfx canister update-settings low_mem_warn --wasm-memory-limit 300MB --wasm-memory-threshold 200MB && dfx canister call low_mem_warn stress_heap_malloc_no_free

// New function: malloc blocks of memory in a loop, never free, do NOT interact with stable memory.
// Allocate heap continuously until malloc fails. Counts ticks to evaluate low-mem trigger conditions.
IC_API_UPDATE(stress_heap_malloc_no_free, ("() -> (nat64)")) {
    ic_api_debug_print(" stress_heap_malloc_no_free called (no stable write, malloc without free)");
    int32_t alloc_ticks = 0;
    const int64_t chunk_size = 262144; // 256KB
    const int64_t max_chunks = (400LL * 1024 * 1024) / chunk_size; // up to 400MB, but keeps going until malloc fails

    while (alloc_ticks < max_chunks) {
        void *buf = malloc(chunk_size);
        if (!buf) {
            char msg[100];
            tfp_snprintf(msg, sizeof(msg), "heap-malloc: malloc failed after %ld ticks", (long)alloc_ticks);
            ic_api_debug_print(msg);
            break;
        }
        memset(buf, (int)(alloc_ticks & 0xFF), (size_t)chunk_size);
        alloc_ticks++;
        // Intentionally not freeing buf (simulate heap pressure)
    }

    char msg[80];
    tfp_snprintf(msg, sizeof(msg), "stress_heap_malloc_no_free finished after %ld ticks", (long)alloc_ticks);
    ic_api_debug_print(msg);

    IC_API_REPLY_NAT(alloc_ticks);
}

// Handler will be triggered automatically when WASM memory is low
IC_EXPORT_ON_LOW_WASM_MEMORY(function_on_low_wasm_memory) {
    ic_api_debug_print("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX on_low_wasm_memory triggered");
}

// Continuously allocate memory from the WASM heap, keeping pointers so the memory is not freed.
// Never free, infinite loop to maximize WASM memory usage, logs progress.
IC_API_UPDATE(stress_heap_malloc_forever, ("() -> (text)")) {
    size_t chunk_size = 1024 * 1024; // 1 MB per allocation
    size_t count = 0;
    // Allocate a pointer array to prevent blocks being garbage collected or released
    void **allocated = malloc(sizeof(void*) * 1024 * 1024); // Reserve space for up to 1M blocks
    if (!allocated) {
        ic_api_debug_print("Failed to allocate pointer array");
        IC_API_REPLY_TEXT("Failed to allocate pointer array");
        return;
    }

    while (1) {
        void *p = malloc(chunk_size);
        if (!p) {
            char msg[100];
            tfp_snprintf(msg, sizeof(msg), "Malloc failed, allocated %zu MB", count);
            ic_api_debug_print(msg);
            break;
        }
        memset(p, 0, chunk_size); // Prevent optimizer from eliminating the allocation
        allocated[count++] = p;

        // Log progress every 10 allocations
        if (count % 10 == 0) {
            char msg[80];
            tfp_snprintf(msg, sizeof(msg), "Allocated %zu MB", count);
            ic_api_debug_print(msg);
        }
    }

    // Never free, infinite loop to keep memory occupied
    while (1) {}
}
