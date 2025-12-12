// Timer API for IC canisters
// Provides functions to schedule one-time and periodic timers
// Similar to ic_cdk_timers in cdk-rs
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ic_types.h"

// Timer ID type (opaque handle)
typedef uint64_t ic_timer_id_t;

// Invalid timer ID
#define IC_TIMER_ID_INVALID ((ic_timer_id_t)0)

// Timer callback function type
// Called when the timer fires
// user_data: user-provided context pointer
typedef void (*ic_timer_callback_t)(void *user_data);

// Timer result codes
typedef enum {
    IC_TIMER_OK = 0,
    IC_TIMER_ERR_INVALID_ARG = 1,
    IC_TIMER_ERR_OUT_OF_MEMORY = 2,
    IC_TIMER_ERR_NOT_FOUND = 3,
} ic_timer_result_t;

// =============================================================================
// Timer Management API
// =============================================================================

// Set a one-time timer
// delay_ns: delay in nanoseconds from now
// callback: function to call when timer fires
// user_data: user-provided context (passed to callback)
// Returns: timer ID on success, IC_TIMER_ID_INVALID on failure
//
// Example:
//   void my_callback(void *data) {
//       ic_api_debug_print("Timer fired!");
//   }
//   ic_timer_id_t id = ic_timer_set(5000000000, my_callback, NULL);
ic_timer_id_t
ic_timer_set(int64_t delay_ns, ic_timer_callback_t callback, void *user_data);

// Set a periodic timer (interval timer)
// interval_ns: interval in nanoseconds between executions
// callback: function to call periodically
// user_data: user-provided context (passed to callback)
// Returns: timer ID on success, IC_TIMER_ID_INVALID on failure
//
// Example:
//   void periodic_callback(void *data) {
//       ic_api_debug_print("Periodic timer fired!");
//   }
//   ic_timer_id_t id = ic_timer_set_interval(10000000000, periodic_callback,
//   NULL);
ic_timer_id_t ic_timer_set_interval(int64_t             interval_ns,
                                    ic_timer_callback_t callback,
                                    void               *user_data);

// Cancel a timer
// timer_id: ID of timer to cancel
// Returns: IC_TIMER_OK on success, IC_TIMER_ERR_NOT_FOUND if timer doesn't
// exist
//
// Example:
//   ic_timer_clear(id);
ic_timer_result_t ic_timer_clear(ic_timer_id_t timer_id);

// =============================================================================
// Internal API (for canister_global_timer implementation)
// =============================================================================

// Process expired timers (called by canister_global_timer)
// This function is called automatically when the global timer fires
// Users should not call this directly
void ic_timer_process_expired(void);
