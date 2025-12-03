// WASI polyfill implementation
#include "ic_wasi_polyfill.h"

#include <stdbool.h>
#include <stdint.h>

#if defined(__wasm32__) || defined(__wasm32_wasi__)
// One-time initialization guard
static bool g_initialized = false;

/// Compiler barrier: prevents reordering of memory operations.
/// Does NOT emit hardware fence instructions.
#define COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")

// WASI polyfill initialization entry point
// Exported as "start" symbol, invoked by WASM runtime during module load
// Prevent LTO from dropping it by marking it as used and externally visible
__attribute__((export_name("start"))) __attribute__((visibility("default")))
__attribute__((used)) __attribute__((noinline)) void
__ic_wasi_polyfill_start(void) {
    // Ensure single initialization
    // raw_init function imported from libic_wasi_polyfill.a library
    if (!g_initialized) {
        // Direct call - LTO must preserve this because function is exported and
        // used
        COMPILER_BARRIER();
        raw_init(NULL, 0);
        COMPILER_BARRIER();
        g_initialized = true;
    }
}

#endif