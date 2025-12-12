// Example: Using IC timer API
//
// This example demonstrates how to use the timer API to schedule
// one-time and periodic timers.

#include "ic_c_sdk.h"
#include <string.h>

IC_CANDID_EXPORT_DID()

#include "idl/candid.h"
#include "ic_simple.h"
// Counter for periodic timer
static uint64_t periodic_count = 0;

// One-time timer callback
void one_time_callback(void *user_data) {
    ic_api_debug_print("One-time timer TIMER_FIRED!");
}

// Periodic timer callback
void periodic_callback(void *user_data) {
    periodic_count++;
    char msg[64];
    // Simple integer to string conversion
    uint64_t n = periodic_count;
    int      i = 0;
    char     buf[32];
    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    msg[0] = '\0';
    strcat(msg, "Periodic timer TIMER_FIRED #");
    // Reverse the number string
    for (int j = i - 1; j >= 0; j--) {
        char c[2] = {buf[j], '\0'};
        strcat(msg, c);
    }
    ic_api_debug_print(msg);
}

// Global timer entry point
IC_EXPORT_GLOBAL_TIMER(global_timer) { ic_timer_process_expired(); }

// Query to get periodic count
IC_API_QUERY(get_periodic_count, "() -> (nat64)") {
    IC_API_REPLY_NAT(periodic_count);
}

// Update to set a one-time timer (5 seconds)
IC_API_UPDATE(set_one_time_timer, ("() -> (nat64)")) {
    ic_timer_id_t id = ic_timer_set(3000000000LL, one_time_callback, NULL);
    if (id == IC_TIMER_ID_INVALID) {
        ic_api_trap("Failed to set one-time timer");
    }
    ic_api_debug_print("One-time timer set for 3 seconds");
    IC_API_REPLY_NAT(id);
}

// Update to set a periodic timer (1  seconds interval)
IC_API_UPDATE(set_periodic_timer, ("() -> (nat64)")) {
    ic_timer_id_t id =
        ic_timer_set_interval(1000000000LL, periodic_callback, NULL);
    if (id == IC_TIMER_ID_INVALID) {
        ic_api_trap("Failed to set periodic timer");
    }
    ic_api_debug_print("Periodic timer set for 1 second intervals");
    IC_API_REPLY_NAT(id);
}

// Update to clear a timer (takes timer ID as argument)
IC_API_UPDATE(clear_timer, ("(nat64) -> (nat64)")) {
    uint64_t timer_id;
    if (ic_api_from_wire_nat(api, &timer_id) != IC_OK) {
        ic_api_trap("Failed to read timer ID");
    }
    ic_timer_result_t result = ic_timer_clear((ic_timer_id_t)timer_id);
    if (result == IC_TIMER_OK) {
        ic_api_debug_print("Timer TIMER_CLEARED");
        IC_API_REPLY_NAT(result);
    } else {
        ic_api_debug_print("Timer TIMER_NOT_FOUND");
        IC_API_REPLY_NAT(-1); // Failure
    }
}
