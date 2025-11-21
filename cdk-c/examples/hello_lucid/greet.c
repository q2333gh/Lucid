// Example: Simple greet canister function
// This demonstrates basic usage of the IC C SDK

#include "greet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ic_c_sdk.h"

// Query function: greet_no_arg (no input, returns hello)
IC_EXPORT_QUERY(greet_no_arg) {
    // Initialize IC API for a query function
    ic_api_t *api = ic_api_init(IC_ENTRY_QUERY, __func__, true);
    if (api == NULL) {
        ic_api_trap("Failed to initialize IC API");
    }

    // Send response with hello message (no input required)
    ic_result_t result = ic_api_to_wire_text(api, "hello world from cdk-c");

    ic_api_free(api);

    if (result != IC_OK) {
        ic_api_trap("Failed to send response");
    }
    ic_api_debug_print("debug print: hello dfx console ");
    printf("stdio print : hello dfx console\n");
}
