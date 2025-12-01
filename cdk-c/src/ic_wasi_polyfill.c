// WASI polyfill implementation
#include "ic_wasi_polyfill.h"

#include <stdbool.h>

#if defined(__wasm32__) || defined(__wasm32_wasi__)
// One-time initialization guard
static bool g_initialized = false;

// WASI polyfill initialization entry point
// Exported as "start" symbol, invoked by WASM runtime during module load
__attribute__((export_name("start"))) __attribute__((used)) __attribute__((noinline)) void
__ic_wasi_polyfill_start(void) {
    // Ensure single initialization
    // raw_init function imported from libic_wasi_polyfill.a library
    if (!g_initialized) {
        // Direct call - LTO must preserve this because function is exported and
        // used
        raw_init(NULL, 0);
        g_initialized = true;
    }
}
#endif