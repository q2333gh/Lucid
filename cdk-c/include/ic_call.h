// Inter-canister call builder pattern
#pragma once

#include "ic_buffer.h"
#include "ic_principal.h"
#include <stdint.h>

// Function pointer type for callbacks
typedef void (*ic_call_cb_t)(void *env);

// Call builder structure
typedef struct {
    ic_principal_t callee;
    char          *method_name;
    size_t         method_name_len;

    ic_buffer_t args;

    uint64_t cycles_high;
    uint64_t cycles_low;

    ic_call_cb_t reply_fun;
    void        *reply_env;

    ic_call_cb_t reject_fun;
    void        *reject_env;

    // Timeout is not directly supported by system API yet in this binding,
    // but we reserve space for it or handling it via timers if needed.
    uint32_t timeout_seconds;
} ic_call_t;

// Initialize a new call builder
// Returns pointer to allocated builder, or NULL on failure
ic_call_t *ic_call_new(const ic_principal_t *callee, const char *method);

// Free the builder resources (does not cancel the call if already performed)
void ic_call_free(ic_call_t *call);

// Set argument data (appends to existing args)
ic_result_t ic_call_with_arg(ic_call_t *call, const uint8_t *data, size_t len);

// Set cycles to send
void ic_call_with_cycles128(ic_call_t *call, uint64_t high, uint64_t low);
void ic_call_with_cycles(ic_call_t *call, uint64_t amount);

// Set reply callback
void ic_call_on_reply(ic_call_t *call, ic_call_cb_t callback, void *env);

// Set reject callback
void ic_call_on_reject(ic_call_t *call, ic_call_cb_t callback, void *env);

// Execute the call
// Returns IC_OK if successfully scheduled
// Note: The call struct can be freed after this returns, as system has copied
// data? Actually, ic0_call_new takes pointers. "The system copies the data
// immediately" -> We need to check IC spec. IC Spec says: "The system copies
// the parameters immediately." So it is safe to free the builder after perform.
ic_result_t ic_call_perform(ic_call_t *call);
