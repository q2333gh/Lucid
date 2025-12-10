#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

#include "idl/candid.h"

// Minimal Hello World
IC_API_QUERY(greet, "() -> (text)") {
    IC_API_REPLY_TEXT("Hello from minimal C canister!");
}
