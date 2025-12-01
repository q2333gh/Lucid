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
ic_api_t *ic_api_init(ic_entry_type_t entry_type, const char *func_name, bool debug);

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
ic_result_t ic_api_from_wire_blob(ic_api_t *api, uint8_t **blob, size_t *blob_len);
ic_result_t ic_api_from_wire_principal(ic_api_t *api, ic_principal_t *principal);

// Serialize outgoing Candid data
ic_result_t ic_api_to_wire_text(ic_api_t *api, const char *text);
ic_result_t ic_api_to_wire_nat(ic_api_t *api, uint64_t value);
ic_result_t ic_api_to_wire_int(ic_api_t *api, int64_t value);
ic_result_t ic_api_to_wire_blob(ic_api_t *api, const uint8_t *blob, size_t blob_len);
ic_result_t ic_api_to_wire_principal(ic_api_t *api, const ic_principal_t *principal);
ic_result_t ic_api_to_wire_empty(ic_api_t *api); // Return empty result

// Low-level reply dispatch (invoked automatically by serialization functions)
ic_result_t ic_api_msg_reply(ic_api_t *api);
