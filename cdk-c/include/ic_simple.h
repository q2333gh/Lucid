#pragma once

#include "ic_api.h"
#include "ic_candid_registry.h"

// =============================================================================
// Simplified High-Level API Macros
// =============================================================================
// These macros abstract away the boilerplate of initializing and freeing
// the ic_api_t context. They provide an 'api' variable to the function body.
//
// Usage:
//   IC_API_QUERY(greet, "() -> (text)") {
//       IC_API_REPLY_TEXT("hello");
//   }

// Internal wrappers to generate entry points and call user implementations
#define IC_API_ENTRY_POINT_QUERY(func_name, sig)                               \
    IC_CANDID_QUERY(func_name, sig)                                            \
    void do_##func_name(ic_api_t *api);                                        \
    __attribute__((export_name("canister_query " #func_name)))                 \
    __attribute__((visibility("default"))) void                                \
    func_name(void) {                                                          \
        ic_api_t *api = ic_api_init(IC_ENTRY_QUERY, #func_name, true);         \
        if (!api)                                                              \
            ic_api_trap("Failed to initialize IC API");                        \
        do_##func_name(api);                                                   \
        ic_api_free(api);                                                      \
    }                                                                          \
    void do_##func_name(ic_api_t *api)

#define IC_API_ENTRY_POINT_UPDATE(func_name, sig)                              \
    IC_CANDID_UPDATE(func_name, sig)                                           \
    void do_##func_name(ic_api_t *api);                                        \
    __attribute__((export_name("canister_update " #func_name)))                \
    __attribute__((visibility("default"))) void                                \
    func_name(void) {                                                          \
        ic_api_t *api = ic_api_init(IC_ENTRY_UPDATE, #func_name, true);        \
        if (!api)                                                              \
            ic_api_trap("Failed to initialize IC API");                        \
        do_##func_name(api);                                                   \
        ic_api_free(api);                                                      \
    }                                                                          \
    void do_##func_name(ic_api_t *api)

// Macro for Query methods
#define IC_API_QUERY(func_name, sig) IC_API_ENTRY_POINT_QUERY(func_name, sig)

// Macro for Update methods
#define IC_API_UPDATE(func_name, sig) IC_API_ENTRY_POINT_UPDATE(func_name, sig)

// =============================================================================
// Convenience Reply Macros
// =============================================================================

#define IC_API_REPLY_TEXT(text) ic_api_to_wire_text(api, text)
#define IC_API_REPLY_NAT(val) ic_api_to_wire_nat(api, val)
#define IC_API_REPLY_INT(val) ic_api_to_wire_int(api, val)
#define IC_API_REPLY_BLOB(data, len) ic_api_to_wire_blob(api, data, len)
#define IC_API_REPLY_PRINCIPAL(p) ic_api_to_wire_principal(api, p)
#define IC_API_REPLY_EMPTY() ic_api_to_wire_empty(api)

// =============================================================================
// Convenience Argument Macros
// =============================================================================

#define IC_API_ARG_TEXT(text_ptr, len_ptr)                                     \
    ic_api_from_wire_text(api, text_ptr, len_ptr)
#define IC_API_ARG_NAT(val_ptr) ic_api_from_wire_nat(api, val_ptr)
#define IC_API_ARG_INT(val_ptr) ic_api_from_wire_int(api, val_ptr)
#define IC_API_ARG_BLOB(blob_ptr, len_ptr)                                     \
    ic_api_from_wire_blob(api, blob_ptr, len_ptr)
#define IC_API_ARG_PRINCIPAL(p_ptr) ic_api_from_wire_principal(api, p_ptr)
