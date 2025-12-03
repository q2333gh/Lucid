// WASI polyfill header: provides WASI compatibility layer for IC environment
#pragma once
#include <stddef.h>

#include "wasm_symbol.h"

// raw_init function imported from libic_wasi_polyfill.a library
#if defined(__wasm32__) || defined(__wasm32_wasi__)
// Prevent LTO from removing this import by marking as used
extern void raw_init(char *p, size_t len)
    WASM_SYMBOL_IMPORTED("polyfill", "raw_init");
#else
// Non-WASM platforms: regular function declaration (may be unused)
void raw_init(char *p, size_t len);
#endif

// WASM-only initialization: polyfill replaces wasi-impl with ic-impl
#if defined(__wasm32__) || defined(__wasm32_wasi__)
// Function declaration - implementation resides in ic_wasi_polyfill.c
// Exported as "start" entry point, invoked by WASM runtime
__attribute__((export_name("start"))) void __ic_wasi_polyfill_start(void);
#endif
