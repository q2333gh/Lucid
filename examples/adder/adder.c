#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

#include "idl/candid.h"
#include <string.h>
#include <tinyprintf.h>
// Minimal Hello World
IC_API_QUERY(greet, "() -> (text)") {
    IC_API_REPLY_TEXT("Hello from minimal C canister!");
}

static unsigned int counter = 0;

IC_API_UPDATE(increment, "() -> (text)") {
    counter++;
    char msg[128] = {0};
    tfp_snprintf(msg, sizeof msg, "increment called, result is: %u", counter);
    ic_api_debug_print(msg);

    char reply[64] = {0};
    tfp_snprintf(reply, sizeof reply, "Incremented! value=%u", counter);
    IC_API_REPLY_TEXT(reply);
}