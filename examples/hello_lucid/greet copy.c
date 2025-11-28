// Example: Simple greet canister function
// This demonstrates basic usage of the IC C SDK

#include "greet.h"

#include <string.h>

#include "ic_c_sdk.h"

// Undefine macros from ic_c_sdk.h (via ic_candid.h) that conflict with
// idl/candid.h
#undef CANDID_TYPE_NULL
#undef CANDID_TYPE_BOOL
#undef CANDID_TYPE_NAT
#undef CANDID_TYPE_INT
#undef CANDID_TYPE_NAT8
#undef CANDID_TYPE_INT8
#undef CANDID_TYPE_NAT16
#undef CANDID_TYPE_INT16
#undef CANDID_TYPE_NAT32
#undef CANDID_TYPE_INT32
#undef CANDID_TYPE_NAT64
#undef CANDID_TYPE_INT64
#undef CANDID_TYPE_FLOAT32
#undef CANDID_TYPE_FLOAT64
#undef CANDID_TYPE_TEXT
#undef CANDID_TYPE_RESERVED
#undef CANDID_TYPE_EMPTY
#undef CANDID_TYPE_OPT
#undef CANDID_TYPE_VEC
#undef CANDID_TYPE_BLOB
#undef CANDID_TYPE_RECORD
#undef CANDID_TYPE_VARIANT
#undef CANDID_TYPE_FUNC
#undef CANDID_TYPE_SERVICE
#undef CANDID_TYPE_PRINCIPAL

#include "candid.h"

// Query function: greet_no_arg (no input, returns hello)
IC_EXPORT_QUERY(greet_no_arg) {
    // Initialize IC API for a query function
    ic_api_t* api = ic_api_init(IC_ENTRY_QUERY, __func__, true);
    if (api == NULL) {
        ic_api_trap("Failed to initialize IC API");
    }

    // Initialize Candid arena and builder
    CandidArena arena;
    CandidArenaInit(&arena, 4096);

    CandidBuilder builder;
    CandidBuilderInit(&builder, &arena);

    // Add return argument: "hello world from cdk-c"
    const char* msg = "hello world from cdk-c";
    CandidBuilderArgText(&builder, msg, strlen(msg));

    // Serialize to bytes
    uint8_t* bytes;
    size_t len;
    if (CandidBuilderSerialize(&builder, &bytes, &len) == CANDID_OK) {
        // Send raw response using system API
        ic0_msg_reply_data_append((uintptr_t)bytes, (uint32_t)len);
        ic0_msg_reply();
    } else {
        ic_api_trap("Failed to serialize response");
    }

    // Clean up
    CandidArenaDestroy(&arena);

    ic_api_debug_print("debug print: hello dfx console ");
    // printf("stdio print : hello dfx console\n");

    ic_api_free(api);
}
