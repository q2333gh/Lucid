// IC System API bindings for C
// This header declares C bindings for IC runtime system calls per spec
// Reference:
// https://internetcomputer.org/docs/current/references/ic-interface-spec#system-api-imports

#pragma once

#include <stddef.h>
#include <stdint.h>

// WASI polyfill must be initialized before using these functions
#include "ic_wasi_polyfill.h"

// For native builds, use stub implementations instead of WASM imports
#ifdef __wasm__
#include "wasm_symbol.h"
#else
// Native build: remove WASM import attributes, use stub implementations
// Undefine first to avoid redefinition warning
#ifdef WASM_SYMBOL_IMPORTED
#undef WASM_SYMBOL_IMPORTED
#endif
#define WASM_SYMBOL_IMPORTED(module, name)
#endif

// Incoming message data and metadata
uint32_t ic0_msg_arg_data_size(void)
    WASM_SYMBOL_IMPORTED("ic0", "msg_arg_data_size");

void ic0_msg_arg_data_copy(uintptr_t dst, uint32_t off, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "msg_arg_data_copy");

uint32_t ic0_msg_caller_size(void)
    WASM_SYMBOL_IMPORTED("ic0", "msg_caller_size");

void ic0_msg_caller_copy(uintptr_t dst, uint32_t off, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "msg_caller_copy");

uint32_t ic0_msg_reject_code(void)
    WASM_SYMBOL_IMPORTED("ic0", "msg_reject_code");

uint32_t ic0_msg_reject_msg_size(void)
    WASM_SYMBOL_IMPORTED("ic0", "msg_reject_msg_size");

void ic0_msg_reject_msg_copy(uintptr_t dst, uint32_t off, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "msg_reject_msg_copy");

int64_t ic0_msg_deadline(void) WASM_SYMBOL_IMPORTED("ic0", "msg_deadline");

// Outgoing message construction and sending
void ic0_msg_reply_data_append(uintptr_t src, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "msg_reply_data_append");

void ic0_msg_reply(void) WASM_SYMBOL_IMPORTED("ic0", "msg_reply");

void ic0_msg_reject(uintptr_t src, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "msg_reject");

// Cycle management operations (128-bit precision)
void ic0_msg_cycles_available128(uintptr_t dst)
    WASM_SYMBOL_IMPORTED("ic0", "msg_cycles_available128");

void ic0_msg_cycles_refunded128(uintptr_t dst)
    WASM_SYMBOL_IMPORTED("ic0", "msg_cycles_refunded128");

void ic0_msg_cycles_accept128(int64_t   max_amount_high,
                              int64_t   max_amount_low,
                              uintptr_t dst)
    WASM_SYMBOL_IMPORTED("ic0", "msg_cycles_accept128");

// Current canister identity and state queries
uint32_t ic0_canister_self_size(void)
    WASM_SYMBOL_IMPORTED("ic0", "canister_self_size");

void ic0_canister_self_copy(uintptr_t dst, uint32_t off, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "canister_self_copy");

void ic0_canister_cycle_balance128(uintptr_t dst)
    WASM_SYMBOL_IMPORTED("ic0", "canister_cycle_balance128");

uint32_t ic0_canister_status(void)
    WASM_SYMBOL_IMPORTED("ic0", "canister_status");

int64_t ic0_canister_version(void)
    WASM_SYMBOL_IMPORTED("ic0", "canister_version");

// Asynchronous canister-to-canister communication
void ic0_call_new(uintptr_t callee_src,
                  uint32_t  callee_size,
                  uintptr_t name_src,
                  uint32_t  name_size,
                  uintptr_t reply_fun,
                  uintptr_t reply_env,
                  uintptr_t reject_fun,
                  uintptr_t reject_env) WASM_SYMBOL_IMPORTED("ic0", "call_new");

void ic0_call_on_cleanup(uintptr_t fun, uintptr_t env)
    WASM_SYMBOL_IMPORTED("ic0", "call_on_cleanup");

void ic0_call_data_append(uintptr_t src, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "call_data_append");

void ic0_call_cycles_add128(int64_t amount_high, int64_t amount_low)
    WASM_SYMBOL_IMPORTED("ic0", "call_cycles_add128");

uint32_t ic0_call_perform(void) WASM_SYMBOL_IMPORTED("ic0", "call_perform");

// Persistent storage API (32-bit addressing, legacy)
uint32_t ic0_stable_size(void) WASM_SYMBOL_IMPORTED("ic0", "stable_size");

uint32_t ic0_stable_grow(uint32_t new_pages)
    WASM_SYMBOL_IMPORTED("ic0", "stable_grow");

void ic0_stable_write(uint32_t off, uintptr_t src, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "stable_write");

void ic0_stable_read(uintptr_t dst, uint32_t off, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "stable_read");

// Persistent storage API (64-bit addressing, preferred)
int64_t ic0_stable64_size(void) WASM_SYMBOL_IMPORTED("ic0", "stable64_size");

int64_t ic0_stable64_grow(int64_t new_pages)
    WASM_SYMBOL_IMPORTED("ic0", "stable64_grow");

// Note: 64-bit stable memory APIs use 64-bit pointer parameters in IC spec.
// Using uint64_t here ensures the import signature matches [I64, I64, I64].
void ic0_stable64_write(int64_t off, uint64_t src, int64_t size)
    WASM_SYMBOL_IMPORTED("ic0", "stable64_write");

void ic0_stable64_read(uint64_t dst, int64_t off, int64_t size)
    WASM_SYMBOL_IMPORTED("ic0", "stable64_read");

// System time and authorization checks
int64_t ic0_time(void) WASM_SYMBOL_IMPORTED("ic0", "time");

// Global timer API
// Sets the global timer to fire at the specified timestamp (nanoseconds since
// epoch) Returns: the previous timer value, or 0 if no timer was set
int64_t ic0_global_timer_set(int64_t timestamp)
    WASM_SYMBOL_IMPORTED("ic0", "global_timer_set");

uint32_t ic0_is_controller(uintptr_t src, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "is_controller");

int32_t ic0_in_replicated_execution(void)
    WASM_SYMBOL_IMPORTED("ic0", "in_replicated_execution");

// HTTP request cost calculation
void ic0_cost_http_request(int64_t   request_size,
                           int64_t   max_response_bytes,
                           uintptr_t dst)
    WASM_SYMBOL_IMPORTED("ic0", "cost_http_request");

// Development and error reporting utilities
void ic0_debug_print(uintptr_t src, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "debug_print");

_Noreturn void ic0_trap(uintptr_t src, uint32_t size)
    WASM_SYMBOL_IMPORTED("ic0", "trap");
