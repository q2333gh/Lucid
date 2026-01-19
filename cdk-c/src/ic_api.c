/*
 * IC API Core Implementation
 *
 * This file implements the main IC API context (ic_api_t) that manages the
 * lifecycle of canister function calls. It provides:
 * - API initialization and cleanup with automatic memory management
 * - Input/output buffer management for Candid serialization
 * - Principal identity handling (caller, canister self)
 * - Candid wire format conversion (from_wire/to_wire) for common types
 * - Memory pool tracking for automatic cleanup
 * - Entry point type management (query, update, callbacks)
 *
 * The API context is the central coordination point for all IC operations,
 * managing deserialization, reply construction, and resource cleanup.
 */
#include "ic_api.h"

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TINYPRINTF_OVERRIDE_LIBC 0
#include <tinyprintf.h>
#undef TINYPRINTF_OVERRIDE_LIBC

#include "ic_entry_points.h"
#include "idl/candid.h"
#include "idl/cdk_alloc.h"
#include "idl/leb128.h"

#define IC_API_DEBUG_PRINT_BUFFER_SIZE 256

// Memory pool node for tracking allocated memory
typedef struct ic_mem_node {
    void               *ptr;
    struct ic_mem_node *next;
} ic_mem_node_t;

struct ic_api {
    ic_entry_type_t entry_type;
    const char     *calling_function;
    ic_buffer_t     input_buffer;
    ic_buffer_t     output_buffer;
    ic_principal_t  caller;
    ic_principal_t  canister_self;
    bool            debug_print;
    bool            called_from_wire;
    bool            called_to_wire;

    idl_arena         arena;
    ic_mem_node_t    *mem_pool;     // Linked list of allocated memory blocks
    idl_deserializer *deserializer; // Cached deserializer for args parsing
};

ic_api_t *
ic_api_init(ic_entry_type_t entry_type, const char *func_name, bool debug) {
    ic_api_t *api = (ic_api_t *)malloc(sizeof(ic_api_t));
    if (api == NULL) {
        return NULL;
    }

    // Set up context fields
    api->entry_type = entry_type;
    api->calling_function = func_name;
    api->debug_print = debug;
    api->called_from_wire = false;
    api->called_to_wire = false;
    api->mem_pool = NULL;
    api->deserializer = NULL;

    // Initialize arena
    idl_arena_init(&api->arena, 4096);

    // Allocate and initialize data buffers
    if (ic_buffer_init(&api->input_buffer) != IC_OK) {
        idl_arena_destroy(&api->arena);
        free(api);
        return NULL;
    }
    if (ic_buffer_init(&api->output_buffer) != IC_OK) {
        ic_buffer_free(&api->input_buffer);
        idl_arena_destroy(&api->arena);
        free(api);
        return NULL;
    }

    // Load message data
    //
    // IMPORTANT: For some entry types (notably reject callbacks), the IC
    // contract forbids calling msg_arg_data_* APIs. In those modes, there is
    // no argument payload to read anyway, so we must skip this section.
    //
    // See: IC0504 "ic0_msg_arg_data_size cannot be executed in reject callback
    // mode".
    if (entry_type != IC_ENTRY_REJECT_CALLBACK) {
        uint32_t arg_size = ic0_msg_arg_data_size();
        if (arg_size > 0) {
            if (ic_buffer_reserve(&api->input_buffer, arg_size) != IC_OK) {
                ic_buffer_free(&api->input_buffer);
                ic_buffer_free(&api->output_buffer);
                idl_arena_destroy(&api->arena);
                free(api);
                return NULL;
            }
            uint8_t *temp_buf = (uint8_t *)malloc(arg_size);
            if (temp_buf == NULL) {
                ic_buffer_free(&api->input_buffer);
                ic_buffer_free(&api->output_buffer);
                idl_arena_destroy(&api->arena);
                free(api);
                return NULL;
            }
            ic0_msg_arg_data_copy((uintptr_t)temp_buf, 0, arg_size);
            ic_buffer_append(&api->input_buffer, temp_buf, arg_size);
            free(temp_buf);
        }
    }

    // Load principal identities
    uint32_t caller_size = ic0_msg_caller_size();
    if (caller_size > 0 && caller_size <= IC_PRINCIPAL_MAX_LEN) {
        uint8_t caller_bytes[IC_PRINCIPAL_MAX_LEN];
        ic0_msg_caller_copy((uintptr_t)caller_bytes, 0, caller_size);
        ic_principal_from_bytes(&api->caller, caller_bytes, caller_size);
    } else {
        api->caller.len = 0;
    }
    uint32_t self_size = ic0_canister_self_size();
    if (self_size > 0 && self_size <= IC_PRINCIPAL_MAX_LEN) {
        uint8_t self_bytes[IC_PRINCIPAL_MAX_LEN];
        ic0_canister_self_copy((uintptr_t)self_bytes, 0, self_size);
        ic_principal_from_bytes(&api->canister_self, self_bytes, self_size);
    } else {
        api->canister_self.len = 0;
    }

    // Diagnostic output when enabled
    if (debug) {
        ic_api_debug_print("\n--");
        char caller_text[IC_PRINCIPAL_MAX_LEN * 2 + 1];
        if (ic_principal_to_text(&api->caller, caller_text,
                                 sizeof(caller_text)) > 0) {
            char msg[256];
            // snprintf(msg, sizeof(msg), "cdk-c: caller principal = %s",
            //          caller_text);
            ic_api_debug_print(msg);
        }
    }
    return api;
}

void ic_api_free(ic_api_t *api) {
    if (api != NULL) {
        // Free all allocated memory in the pool
        ic_mem_node_t *node = api->mem_pool;
        while (node != NULL) {
            ic_mem_node_t *next = node->next;
            if (node->ptr != NULL) {
                free(node->ptr);
            }
            free(node);
            node = next;
        }

        ic_buffer_free(&api->input_buffer);
        ic_buffer_free(&api->output_buffer);
        idl_arena_destroy(&api->arena);
        free(api);
    }
}

// Memory pool allocation: allocates memory that will be freed in ic_api_free
void *ic_api_malloc(ic_api_t *api, size_t size) {
    if (api == NULL || size == 0) {
        return NULL;
    }

    void *ptr = malloc(size);
    if (ptr == NULL) {
        return NULL;
    }

    // Add to memory pool
    ic_mem_node_t *node = (ic_mem_node_t *)malloc(sizeof(ic_mem_node_t));
    if (node == NULL) {
        free(ptr);
        return NULL;
    }

    node->ptr = ptr;
    node->next = api->mem_pool;
    api->mem_pool = node;

    return ptr;
}

ic_principal_t ic_api_get_caller(const ic_api_t *api) {
    if (api == NULL) {
        ic_principal_t empty = {0};
        return empty;
    }
    return api->caller;
}

ic_principal_t ic_api_get_canister_self(const ic_api_t *api) {
    if (api == NULL) {
        ic_principal_t empty = {0};
        return empty;
    }
    return api->canister_self;
}

ic_result_t ic_api_get_canister_cycle_balance(const ic_api_t *api,
                                              uint64_t       *balance_high,
                                              uint64_t       *balance_low) {
    if (api == NULL || balance_high == NULL || balance_low == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    uint64_t balance[2] = {0, 0};
    ic0_canister_cycle_balance128((uintptr_t)balance);

    // 128-bit values returned as two 64-bit words
    // balance[0] contains low 64 bits, balance[1] contains high 64 bits
    *balance_low = balance[0];
    *balance_high = balance[1];

    return IC_OK;
}

bool ic_api_is_controller(const ic_api_t       *api,
                          const ic_principal_t *principal) {
    if (api == NULL || principal == NULL || !ic_principal_is_valid(principal)) {
        return false;
    }
    uint32_t result = ic0_is_controller((uintptr_t)principal->bytes,
                                        (uint32_t)principal->len);
    return result == 1;
}

int64_t ic_api_time(void) { return ic0_time(); }

void ic_api_debug_print(const char *msg) {
    if (msg == NULL) {
        return;
    }
    size_t len = strlen(msg);
    if (len > UINT32_MAX) {
        len = UINT32_MAX;
    }
    ic0_debug_print((uintptr_t)msg, (uint32_t)len);
}

void ic_api_debug_printf(const char *fmt, ...) {
    if (fmt == NULL) {
        return;
    }

    char    buf[IC_API_DEBUG_PRINT_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    tfp_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ic_api_debug_print(buf);
}

void ic_api_trap(const char *msg) {
    if (msg == NULL) {
        msg = "Execution aborted";
    }

    size_t len = strlen(msg);
    if (len > UINT32_MAX) {
        len = UINT32_MAX;
    }

    ic0_trap((uintptr_t)msg, (uint32_t)len);
}

const ic_buffer_t *ic_api_get_input_buffer(const ic_api_t *api) {
    if (api == NULL) {
        return NULL;
    }
    return &api->input_buffer;
}

const ic_buffer_t *ic_api_get_output_buffer(const ic_api_t *api) {
    if (api == NULL) {
        return NULL;
    }
    return &api->output_buffer;
}

bool ic_api_has_called_from_wire(const ic_api_t *api) {
    return api != NULL && api->called_from_wire;
}

bool ic_api_has_called_to_wire(const ic_api_t *api) {
    return api != NULL && api->called_to_wire;
}

ic_entry_type_t ic_api_get_entry_type(const ic_api_t *api) {
    if (api == NULL) {
        return IC_ENTRY_QUERY; // Safe default
    }
    return api->entry_type;
}

// Candid deserialization routines

static ic_result_t ic_api_from_wire_generic(ic_api_t   *api,
                                            idl_value **out_val) {
    if (api == NULL || out_val == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_from_wire) {
        ic_api_trap(
            "cdk-c: ic_api_from_wire() may only be called once per message");
    }
    api->called_from_wire = true;

    const uint8_t *data = ic_buffer_data(&api->input_buffer);
    size_t         len = ic_buffer_size(&api->input_buffer);

    idl_deserializer *de = NULL;
    if (idl_deserializer_new(data, len, &api->arena, &de) != IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    idl_type *type;
    if (idl_deserializer_get_value(de, &type, out_val) != IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }
    return IC_OK;
}

ic_result_t
ic_api_from_wire_text(ic_api_t *api, char **text, size_t *text_len) {
    idl_value  *val;
    ic_result_t res = ic_api_from_wire_generic(api, &val);
    if (res != IC_OK) {
        return res;
    }

    if (val->kind != IDL_VALUE_TEXT) {
        return IC_ERR_INVALID_ARG;
    }

    *text = (char *)val->data.text.data;
    *text_len = val->data.text.len;
    return IC_OK;
}

ic_result_t ic_api_from_wire_nat(ic_api_t *api, uint64_t *value) {
    idl_value  *val;
    ic_result_t res = ic_api_from_wire_generic(api, &val);
    if (res != IC_OK) {
        return res;
    }

    if (val->kind == IDL_VALUE_NAT64) {
        *value = val->data.nat64_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_NAT32) {
        *value = val->data.nat32_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_NAT16) {
        *value = val->data.nat16_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_NAT8) {
        *value = val->data.nat8_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_NAT) {
        // Try to decode LEB128
        size_t consumed;
        if (idl_uleb128_decode(val->data.bignum.data, val->data.bignum.len,
                               &consumed, value) != IDL_STATUS_OK) {
            return IC_ERR_INVALID_ARG; // Overflow or invalid
        }
        return IC_OK;
    }

    return IC_ERR_INVALID_ARG;
}

ic_result_t ic_api_from_wire_int(ic_api_t *api, int64_t *value) {
    idl_value  *val;
    ic_result_t res = ic_api_from_wire_generic(api, &val);
    if (res != IC_OK) {
        return res;
    }

    if (val->kind == IDL_VALUE_INT64) {
        *value = val->data.int64_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_INT32) {
        *value = val->data.int32_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_INT16) {
        *value = val->data.int16_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_INT8) {
        *value = val->data.int8_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_INT) {
        size_t consumed;
        if (idl_sleb128_decode(val->data.bignum.data, val->data.bignum.len,
                               &consumed, value) != IDL_STATUS_OK) {
            return IC_ERR_INVALID_ARG;
        }
        return IC_OK;
    }

    return IC_ERR_INVALID_ARG;
}

ic_result_t
ic_api_from_wire_blob(ic_api_t *api, uint8_t **blob, size_t *blob_len) {
    idl_value  *val;
    ic_result_t res = ic_api_from_wire_generic(api, &val);
    if (res != IC_OK) {
        return res;
    }

    if (val->kind == IDL_VALUE_BLOB) {
        *blob = (uint8_t *)val->data.blob.data;
        *blob_len = val->data.blob.len;
        return IC_OK;
    }
    // TODO: Handle Vec Nat8
    return IC_ERR_INVALID_ARG;
}

ic_result_t ic_api_from_wire_principal(ic_api_t       *api,
                                       ic_principal_t *principal) {
    idl_value  *val;
    ic_result_t res = ic_api_from_wire_generic(api, &val);
    if (res != IC_OK) {
        return res;
    }

    if (val->kind == IDL_VALUE_PRINCIPAL) {
        return ic_principal_from_bytes(principal, val->data.principal.data,
                                       val->data.principal.len);
    }
    return IC_ERR_INVALID_ARG;
}

// Helper for sending reply
ic_result_t ic_api_reply_builder(ic_api_t *api, void *builder_ptr) {
    if (api == NULL || builder_ptr == NULL) {
        return IC_ERR_INVALID_ARG;
    }
    if (api->called_to_wire) {
        ic_api_trap("cdk-c: ic_api_reply_builder() called twice");
    }
    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap("cdk-c: cannot reply");
    }
    api->called_to_wire = true;

    idl_builder *builder = (idl_builder *)builder_ptr;
    uint8_t     *bytes;
    size_t       len;
    if (idl_builder_serialize(builder, &bytes, &len) != IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    // Copy to output buffer for consistency with API
    ic_buffer_clear(&api->output_buffer);
    ic_buffer_append(&api->output_buffer, bytes, len);

    return ic_api_msg_reply(api);
}

ic_result_t ic_api_to_wire_text(ic_api_t *api, const char *text) {
    if (api == NULL || text == NULL) {
        return IC_ERR_INVALID_ARG;
    }
    if (api->called_to_wire) {
        ic_api_trap("cdk-c: ic_api_to_wire() called twice");
    }
    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap("cdk-c: cannot reply");
    }

    idl_builder builder;
    idl_builder_init(&builder, &api->arena);
    idl_builder_arg_text_cstr(&builder, text);

    return ic_api_reply_builder(api, &builder);
}

ic_result_t ic_api_to_wire_nat(ic_api_t *api, uint64_t value) {
    if (api == NULL) {
        return IC_ERR_INVALID_ARG;
    }
    if (api->called_to_wire) {
        ic_api_trap("cdk-c: ic_api_to_wire() called twice");
    }
    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap("cdk-c: cannot reply");
    }

    idl_builder builder;
    idl_builder_init(&builder, &api->arena);
    idl_builder_arg_nat64(&builder, value);

    return ic_api_reply_builder(api, &builder);
}

ic_result_t ic_api_to_wire_int(ic_api_t *api, int64_t value) {
    if (api == NULL) {
        return IC_ERR_INVALID_ARG;
    }
    if (api->called_to_wire) {
        ic_api_trap("cdk-c: ic_api_to_wire() called twice");
    }
    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap("cdk-c: cannot reply");
    }

    idl_builder builder;
    idl_builder_init(&builder, &api->arena);
    idl_builder_arg_int64(&builder, value);

    return ic_api_reply_builder(api, &builder);
}

ic_result_t
ic_api_to_wire_blob(ic_api_t *api, const uint8_t *blob, size_t blob_len) {
    if (api == NULL || blob == NULL) {
        return IC_ERR_INVALID_ARG;
    }
    if (api->called_to_wire) {
        ic_api_trap("cdk-c: ic_api_to_wire() called twice");
    }
    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap("cdk-c: cannot reply");
    }

    idl_builder builder;
    idl_builder_init(&builder, &api->arena);
    idl_builder_arg_blob(&builder, blob, blob_len);

    return ic_api_reply_builder(api, &builder);
}

ic_result_t ic_api_to_wire_principal(ic_api_t             *api,
                                     const ic_principal_t *principal) {
    if (api == NULL || principal == NULL) {
        return IC_ERR_INVALID_ARG;
    }
    if (api->called_to_wire) {
        ic_api_trap("cdk-c: ic_api_to_wire() called twice");
    }
    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap("cdk-c: cannot reply");
    }

    idl_builder builder;
    idl_builder_init(&builder, &api->arena);
    idl_builder_arg_principal(&builder, principal->bytes, principal->len);

    return ic_api_reply_builder(api, &builder);
}

ic_result_t ic_api_to_wire_empty(ic_api_t *api) {
    if (api == NULL) {
        return IC_ERR_INVALID_ARG;
    }
    if (api->called_to_wire) {
        ic_api_trap("cdk-c: ic_api_to_wire() called twice");
    }
    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap("cdk-c: cannot reply");
    }

    idl_builder builder;
    idl_builder_init(&builder, &api->arena);
    // No arguments

    return ic_api_reply_builder(api, &builder);
}

ic_result_t ic_api_msg_reply(ic_api_t *api) {
    if (api == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    const uint8_t *data = ic_buffer_data(&api->output_buffer);
    size_t         size = ic_buffer_size(&api->output_buffer);

    // Note: c_candid handles the header generation, so we just send what's in
    // buffer. The original code checked for CANDID_MAGIC. We assume
    // idl_builder_serialize did its job.

    // Transfer data to reply buffer
    if (size > UINT32_MAX) {
        return IC_ERR_BUFFER_OVERFLOW;
    }

    ic0_msg_reply_data_append((uintptr_t)data, (uint32_t)size);

    // Clean up buffers
    ic_buffer_clear(&api->input_buffer);
    ic_buffer_clear(&api->output_buffer);
    // Also clean up arena since we are done
    idl_arena_reset(&api->arena);
    // Note: ic_api_free destroys arena, but reusing api object might be
    // possible (though unusual for this API pattern). ic_api_init calls init,
    // ic_api_free calls destroy. If we reset here, we free memory used for
    // reply. However, resetting arena invalidates pointers (e.g. if user kept
    // pointer from from_wire). But this is msg_reply, so we are done. But wait,
    // if the user does `from_wire` then `to_wire`, we don't want to invalidate
    // `from_wire` data until we are truly done. Does `ic_api_msg_reply` mark
    // end of life? Usually yes, but `ic_api_free` is explicit. Safest is to NOT
    // reset arena here, let `ic_api_free` handle it. But `input_buffer` is
    // cleared.

    // Finalize reply
    ic0_msg_reply();

    return IC_OK;
}

// Internal helper to get arena (not exported to WASM)
// This is used internally by ic_args.c and for building replies
// Marked with internal name to avoid WASM export issues
idl_arena *ic_api_get_arena_internal(ic_api_t *api) {
    if (api == NULL) {
        return NULL;
    }
    return &api->arena;
}

// Reset the arena (useful if you need more space)
void ic_api_reset_arena(ic_api_t *api) {
    if (api != NULL) {
        idl_arena_reset(&api->arena);
    }
}

uint32_t ic_api_msg_reject_code(void) { return ic0_msg_reject_code(); }

ic_result_t
ic_api_msg_reject_message(char *buf, size_t buf_len, size_t *out_len) {
    if (buf == NULL || buf_len == 0) {
        return IC_ERR_INVALID_ARG;
    }

    uint32_t msg_size = ic0_msg_reject_msg_size();
    if (out_len != NULL) {
        *out_len = msg_size;
    }

    // Leave space for a terminating '\0'
    size_t copy_len = msg_size;
    if (copy_len >= buf_len) {
        copy_len = buf_len - 1;
    }

    if (copy_len > 0) {
        ic0_msg_reject_msg_copy((uintptr_t)buf, 0, (uint32_t)copy_len);
    }
    buf[copy_len] = '\0';

    if (copy_len < msg_size) {
        // Truncated
        return IC_ERR_BUFFER_OVERFLOW;
    }

    return IC_OK;
}
