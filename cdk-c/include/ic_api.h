// Convenience API for IC interactions in C programs

#pragma once

#include <stdbool.h>

#include "ic0.h"
#include "ic_buffer.h"
#include "ic_entry_points.h"
#include "ic_principal.h"
#include "ic_types.h"

// Opaque structure declaration
struct ic_api;

// API context handle type
typedef struct ic_api ic_api_t;

// Create and initialize an API context
// entry_type: entry point classification (IC_ENTRY_QUERY, IC_ENTRY_UPDATE,
// etc.) func_name: function identifier for diagnostic purposes debug: whether
// to enable diagnostic output
ic_api_t *
ic_api_init(ic_entry_type_t entry_type, const char *func_name, bool debug);

// Release API context and associated resources
void ic_api_free(ic_api_t *api);

// Retrieve the principal of the message sender
ic_principal_t ic_api_get_caller(const ic_api_t *api);

// Retrieve this canister's own principal identifier
ic_principal_t ic_api_get_canister_self(const ic_api_t *api);

// Query this canister's cycle balance (128-bit value)
// Returns IC_OK if successful, IC_ERR otherwise
ic_result_t ic_api_get_canister_cycle_balance(const ic_api_t *api,
                                              uint64_t       *balance_high,
                                              uint64_t       *balance_low);

// Determine if given principal has controller privileges
bool ic_api_is_controller(const ic_api_t *api, const ic_principal_t *principal);

// Obtain current system time (nanoseconds since Unix epoch)
int64_t ic_api_time(void);

// Emit diagnostic message to console
void ic_api_debug_print(const char *msg);

// Attribute helper for printf-style formatting checks
#if defined(__GNUC__) || defined(__clang__)
#define IC_API_PRINTF_ATTR(fmt_idx, arg_idx)                                   \
    __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define IC_API_PRINTF_ATTR(fmt_idx, arg_idx)
#endif

// Emit formatted diagnostic message to console
void ic_api_debug_printf(const char *fmt, ...) IC_API_PRINTF_ATTR(1, 2);
#undef IC_API_PRINTF_ATTR

// Abort execution with error message
_Noreturn void ic_api_trap(const char *msg);

// Access raw input data buffer (expert use only)
const ic_buffer_t *ic_api_get_input_buffer(const ic_api_t *api);

// Access raw output data buffer (expert use only)
const ic_buffer_t *ic_api_get_output_buffer(const ic_api_t *api);

// Query whether deserialization has occurred
bool ic_api_has_called_from_wire(const ic_api_t *api);

// Query whether serialization has occurred
bool ic_api_has_called_to_wire(const ic_api_t *api);

// Retrieve the entry point type for this context
ic_entry_type_t ic_api_get_entry_type(const ic_api_t *api);

// Candid serialization/deserialization routines
// All functions work with Candid binary format including DIDL header

// Deserialize incoming Candid data
// Supported primitive types: text, nat, int, blob, principal
ic_result_t ic_api_from_wire_text(ic_api_t *api, char **text, size_t *text_len);
ic_result_t ic_api_from_wire_nat(ic_api_t *api, uint64_t *value);
ic_result_t ic_api_from_wire_int(ic_api_t *api, int64_t *value);
ic_result_t
ic_api_from_wire_blob(ic_api_t *api, uint8_t **blob, size_t *blob_len);
ic_result_t ic_api_from_wire_principal(ic_api_t       *api,
                                       ic_principal_t *principal);

// Serialize outgoing Candid data
ic_result_t ic_api_to_wire_text(ic_api_t *api, const char *text);
ic_result_t ic_api_to_wire_nat(ic_api_t *api, uint64_t value);
ic_result_t ic_api_to_wire_int(ic_api_t *api, int64_t value);
ic_result_t
ic_api_to_wire_blob(ic_api_t *api, const uint8_t *blob, size_t blob_len);
ic_result_t ic_api_to_wire_principal(ic_api_t             *api,
                                     const ic_principal_t *principal);
ic_result_t ic_api_to_wire_empty(ic_api_t *api); // Return empty result

// Low-level reply dispatch (invoked automatically by serialization functions)
ic_result_t ic_api_msg_reply(ic_api_t *api);

// Reply using a builder (for complex types like records, variants, vec, opt)
// This function serializes the builder and sends the reply
// Note: Include idl/candid.h in your source file to use idl_builder
// The builder parameter type is void* to avoid requiring idl/candid.h in header
ic_result_t ic_api_reply_builder(ic_api_t *api, void *builder_ptr);

// Retrieve reject information for the *current* message (typically in a
// reject callback). These wrap the raw ic0 msg_reject_* APIs.
uint32_t ic_api_msg_reject_code(void);
ic_result_t
ic_api_msg_reject_message(char *buf, size_t buf_len, size_t *out_len);

// Memory pool management (internal use, but exposed for args parsing)
// Allocates memory that will be automatically freed when ic_api_free is called
void *ic_api_malloc(ic_api_t *api, size_t size);

// Forward declarations
struct idl_arena;
struct idl_builder;
typedef struct idl_arena   idl_arena;
typedef struct idl_builder idl_builder;

// Get the arena from API context (for building replies)
// The arena is automatically initialized and will be freed in ic_api_free
// Note: This is an internal function - users should use IC_API_ARENA macro
// instead Internal implementation (not exported to WASM to avoid import errors)
idl_arena *ic_api_get_arena_internal(ic_api_t *api);

// Public API wrapper (macro to avoid WASM export)
#define ic_api_get_arena(api) ic_api_get_arena_internal(api)

// Reset the arena (useful if you need more space)
// This clears all allocations in the arena but keeps it initialized
void ic_api_reset_arena(ic_api_t *api);
