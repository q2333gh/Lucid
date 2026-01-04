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
// Convenience Argument Macros (Legacy - use ic_args.h for new code)
// =============================================================================

#define IC_API_ARG_TEXT(text_ptr, len_ptr)                                     \
    ic_api_from_wire_text(api, text_ptr, len_ptr)
#define IC_API_ARG_NAT(val_ptr) ic_api_from_wire_nat(api, val_ptr)
#define IC_API_ARG_INT(val_ptr) ic_api_from_wire_int(api, val_ptr)
#define IC_API_ARG_BLOB(blob_ptr, len_ptr)                                     \
    ic_api_from_wire_blob(api, blob_ptr, len_ptr)
#define IC_API_ARG_PRINCIPAL(p_ptr) ic_api_from_wire_principal(api, p_ptr)

// =============================================================================
// Simplified Argument Parsing (Recommended for new code)
// Include ic_args.h to use these macros
// =============================================================================
// Usage examples:
//
// Single argument:
//   char *name;
//   IC_API_PARSE_SIMPLE(api, text, name);
//
// Multiple arguments:
//   char *name;
//   uint64_t age;
//   bool active;
//   IC_API_PARSE_BEGIN(api);
//   IC_ARG_TEXT(&name);
//   IC_ARG_NAT(&age);
//   IC_ARG_BOOL(&active);
//   IC_API_PARSE_END();

// =============================================================================
// Simplified Arena Management (Internal - users should not need this)
// =============================================================================
// Get arena pointer from API (automatically managed, no need to init/destroy)
// This uses the internal function to avoid WASM export issues
// NOTE: Users should use IC_API_BUILDER_BEGIN/END macros instead
#define IC_API_ARENA(api) ic_api_get_arena_internal(api)

// Reset arena if you need more space (optional, usually not needed)
#define IC_API_RESET_ARENA(api) ic_api_reset_arena(api)

// =============================================================================
// Simplified Builder API (Fully Automatic - no BEGIN/END needed!)
// =============================================================================
// The 'arena' and 'builder' variables are automatically available in all
// IC_API_QUERY and IC_API_UPDATE functions. Just use them directly!
//
// Usage for building complex replies:
//   IC_API_QUERY(my_query, "(text) -> (record {...})") {
//       // Parse arguments
//       char *name = NULL;
//       IC_API_PARSE_SIMPLE(api, text, name);
//
//       // Build reply - arena and builder are already available!
//       idl_type *my_type = build_my_type(arena);
//       idl_value *my_value = build_my_value(arena);
//       idl_builder_arg(builder, my_type, my_value);
//
//       // Send reply (call this at the end if you used builder)
//       IC_API_REPLY_BUILDER();
//   }
//
// Note: If you use simple reply macros (IC_API_REPLY_TEXT, etc.), you don't
// need to call IC_API_REPLY_BUILDER(). Only call it if you manually used
// idl_builder_arg() to build a complex reply.
#define IC_API_REPLY_BUILDER() ic_api_reply_builder(api, (void *)builder)

// Legacy macros (still supported, but not needed anymore)
// These are kept for backward compatibility
#define IC_API_BUILDER_BEGIN(api)                                              \
    do {                                                                       \
        idl_arena  *_arena = IC_API_ARENA(api);                                \
        idl_builder _builder;                                                  \
        if (idl_builder_init(&_builder, _arena) != IDL_STATUS_OK) {            \
            ic_api_trap("Failed to initialize builder");                       \
        }                                                                      \
        idl_builder *builder = &_builder;                                      \
        idl_arena   *arena = _arena;                                           \
        do

#define IC_API_BUILDER_END(api)                                                \
    while (0)                                                                  \
        ;                                                                      \
    if (ic_api_reply_builder(api, (void *)&_builder) != IC_OK) {               \
        ic_api_trap("cdk-c: ic_api_reply_builder() failed");                   \
    }                                                                          \
    }                                                                          \
    while (0)
