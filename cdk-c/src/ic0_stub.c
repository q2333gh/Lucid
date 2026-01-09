/*
 * IC0 API Stub Implementation
 *
 * Provides stub implementations of all IC0 system API functions for native
 * (non-WASM) builds. These functions return safe defaults or no-ops,
 * allowing the SDK to be compiled and tested in native environments
 * without requiring an actual IC runtime.
 *
 * Used for unit testing, static analysis, and development workflows where
 * full IC execution is not needed.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Stub implementations return safe defaults
uint32_t ic0_msg_arg_data_size(void) { return 0; }
void     ic0_msg_arg_data_copy(uintptr_t dst, uint32_t off, uint32_t size) {
        (void)dst;
        (void)off;
        (void)size;
}
uint32_t ic0_msg_caller_size(void) { return 0; }
void     ic0_msg_caller_copy(uintptr_t dst, uint32_t off, uint32_t size) {
        (void)dst;
        (void)off;
        (void)size;
}
uint32_t ic0_msg_reject_code(void) { return 0; }
uint32_t ic0_msg_reject_msg_size(void) { return 0; }
void     ic0_msg_reject_msg_copy(uintptr_t dst, uint32_t off, uint32_t size) {
        (void)dst;
        (void)off;
        (void)size;
}
int64_t ic0_msg_deadline(void) { return 0; }
void    ic0_msg_reply_data_append(uintptr_t src, uint32_t size) {
       (void)src;
       (void)size;
}
void ic0_msg_reply(void) {}
void ic0_msg_reject(uintptr_t src, uint32_t size) {
    (void)src;
    (void)size;
}
void ic0_msg_cycles_available128(uintptr_t dst) { memset((void *)dst, 0, 16); }
void ic0_msg_cycles_refunded128(uintptr_t dst) { memset((void *)dst, 0, 16); }
void ic0_msg_cycles_accept128(int64_t   max_amount_high,
                              int64_t   max_amount_low,
                              uintptr_t dst) {
    (void)max_amount_high;
    (void)max_amount_low;
    memset((void *)dst, 0, 16);
}
uint32_t ic0_canister_self_size(void) { return 0; }
void     ic0_canister_self_copy(uintptr_t dst, uint32_t off, uint32_t size) {
        (void)dst;
        (void)off;
        (void)size;
}
void ic0_canister_cycle_balance128(uintptr_t dst) {
    memset((void *)dst, 0, 16);
}
uint32_t ic0_canister_status(void) { return 0; }
int64_t  ic0_canister_version(void) { return 0; }
void     ic0_call_new(uintptr_t callee_src,
                      uint32_t  callee_size,
                      uintptr_t name_src,
                      uint32_t  name_size,
                      uintptr_t reply_fun,
                      uintptr_t reply_env,
                      uintptr_t reject_fun,
                      uintptr_t reject_env) {
        (void)callee_src;
        (void)callee_size;
        (void)name_src;
        (void)name_size;
        (void)reply_fun;
        (void)reply_env;
        (void)reject_fun;
        (void)reject_env;
}
void ic0_call_on_cleanup(uintptr_t fun, uintptr_t env) {
    (void)fun;
    (void)env;
}
void ic0_call_data_append(uintptr_t src, uint32_t size) {
    (void)src;
    (void)size;
}
void ic0_call_cycles_add128(int64_t amount_high, int64_t amount_low) {
    (void)amount_high;
    (void)amount_low;
}
uint32_t ic0_call_perform(void) { return 0; }
uint32_t ic0_stable_size(void) { return 0; }
uint32_t ic0_stable_grow(uint32_t new_pages) {
    (void)new_pages;
    return 0;
}
void ic0_stable_write(uint32_t off, uintptr_t src, uint32_t size) {
    (void)off;
    (void)src;
    (void)size;
}
void ic0_stable_read(uintptr_t dst, uint32_t off, uint32_t size) {
    (void)dst;
    (void)off;
    (void)size;
}
int64_t ic0_stable64_size(void) { return 0; }
int64_t ic0_stable64_grow(int64_t new_pages) {
    (void)new_pages;
    return 0;
}
void ic0_stable64_write(int64_t off, uint64_t src, int64_t size) {
    (void)off;
    (void)src;
    (void)size;
}
void ic0_stable64_read(uint64_t dst, int64_t off, int64_t size) {
    (void)dst;
    (void)off;
    (void)size;
}
int64_t ic0_time(void) { return 0; }
int64_t ic0_global_timer_set(int64_t timestamp) {
    (void)timestamp;
    return 0;
}
uint32_t ic0_is_controller(uintptr_t src, uint32_t size) {
    (void)src;
    (void)size;
    return 0;
}
int32_t ic0_in_replicated_execution(void) { return 0; }
void    ic0_debug_print(uintptr_t src, uint32_t size) {
       (void)src;
       (void)size;
}
_Noreturn void ic0_trap(uintptr_t src, uint32_t size) {
    (void)src;
    (void)size;
    abort();
}
