// IC API implementation
#include "ic_api.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
// #include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ic_candid.h"
#include "ic_entry_points.h"

struct ic_api {
    ic_entry_type_t entry_type;
    const char *calling_function;
    ic_buffer_t input_buffer;
    ic_buffer_t output_buffer;
    ic_principal_t caller;
    ic_principal_t canister_self;
    bool debug_print;
    bool called_from_wire;
    bool called_to_wire;
};

ic_api_t *ic_api_init(ic_entry_type_t entry_type, const char *func_name,
                      bool debug) {
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

    // Allocate and initialize data buffers
    if (ic_buffer_init(&api->input_buffer) != IC_OK) {
        free(api);
        return NULL;
    }
    if (ic_buffer_init(&api->output_buffer) != IC_OK) {
        ic_buffer_free(&api->input_buffer);
        free(api);
        return NULL;
    }

    // Load message data
    uint32_t arg_size = ic0_msg_arg_data_size();
    if (arg_size > 0) {
        if (ic_buffer_reserve(&api->input_buffer, arg_size) != IC_OK) {
            ic_buffer_free(&api->input_buffer);
            ic_buffer_free(&api->output_buffer);
            free(api);
            return NULL;
        }
        uint8_t *temp_buf = (uint8_t *)malloc(arg_size);
        if (temp_buf == NULL) {
            ic_buffer_free(&api->input_buffer);
            ic_buffer_free(&api->output_buffer);
            free(api);
            return NULL;
        }
        ic0_msg_arg_data_copy((uintptr_t)temp_buf, 0, arg_size);
        ic_buffer_append(&api->input_buffer, temp_buf, arg_size);
        free(temp_buf);
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
                    //  caller_text);
            ic_api_debug_print(msg);
        }
    }
    return api;
}

void ic_api_free(ic_api_t *api) {
    if (api != NULL) {
        ic_buffer_free(&api->input_buffer);
        ic_buffer_free(&api->output_buffer);
        free(api);
    }
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
                                              uint64_t *balance_high,
                                              uint64_t *balance_low) {
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

bool ic_api_is_controller(const ic_api_t *api,
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
        return IC_ENTRY_QUERY;  // Safe default
    }
    return api->entry_type;
}

// Candid deserialization routines

ic_result_t ic_api_from_wire_text(ic_api_t *api, char **text,
                                  size_t *text_len) {
    if (api == NULL || text == NULL || text_len == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_from_wire) {
        ic_api_trap(
            "cdk-c: ic_api_from_wire() may only be called once per message");
    }

    api->called_from_wire = true;

    const uint8_t *data = ic_buffer_data(&api->input_buffer);
    size_t len = ic_buffer_size(&api->input_buffer);

    // Advance past format header if present
    size_t offset = 0;
    if (len >= CANDID_MAGIC_SIZE && candid_check_magic(data, len)) {
        offset = CANDID_MAGIC_SIZE;
    }

    return candid_deserialize_text(data, len, &offset, text, text_len);
}

ic_result_t ic_api_from_wire_nat(ic_api_t *api, uint64_t *value) {
    if (api == NULL || value == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_from_wire) {
        ic_api_trap(
            "cdk-c: ic_api_from_wire() may only be called once per message");
    }

    api->called_from_wire = true;

    const uint8_t *data = ic_buffer_data(&api->input_buffer);
    size_t len = ic_buffer_size(&api->input_buffer);

    // Advance past format header if present
    size_t offset = 0;
    if (len >= CANDID_MAGIC_SIZE && candid_check_magic(data, len)) {
        offset = CANDID_MAGIC_SIZE;
    }

    return candid_deserialize_nat(data, len, &offset, value);
}

ic_result_t ic_api_from_wire_int(ic_api_t *api, int64_t *value) {
    if (api == NULL || value == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_from_wire) {
        ic_api_trap(
            "cdk-c: ic_api_from_wire() may only be called once per message");
    }

    api->called_from_wire = true;

    const uint8_t *data = ic_buffer_data(&api->input_buffer);
    size_t len = ic_buffer_size(&api->input_buffer);

    // Advance past format header if present
    size_t offset = 0;
    if (len >= CANDID_MAGIC_SIZE && candid_check_magic(data, len)) {
        offset = CANDID_MAGIC_SIZE;
    }

    return candid_deserialize_int(data, len, &offset, value);
}

ic_result_t ic_api_from_wire_blob(ic_api_t *api, uint8_t **blob,
                                  size_t *blob_len) {
    if (api == NULL || blob == NULL || blob_len == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_from_wire) {
        ic_api_trap(
            "cdk-c: ic_api_from_wire() may only be called once per message");
    }

    api->called_from_wire = true;

    const uint8_t *data = ic_buffer_data(&api->input_buffer);
    size_t len = ic_buffer_size(&api->input_buffer);

    // Advance past format header if present
    size_t offset = 0;
    if (len >= CANDID_MAGIC_SIZE && candid_check_magic(data, len)) {
        offset = CANDID_MAGIC_SIZE;
    }

    return candid_deserialize_blob(data, len, &offset, blob, blob_len);
}

ic_result_t ic_api_from_wire_principal(ic_api_t *api,
                                       ic_principal_t *principal) {
    if (api == NULL || principal == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_from_wire) {
        ic_api_trap(
            "cdk-c: ic_api_from_wire() may only be called once per message");
    }

    api->called_from_wire = true;

    const uint8_t *data = ic_buffer_data(&api->input_buffer);
    size_t len = ic_buffer_size(&api->input_buffer);

    // Advance past format header if present
    size_t offset = 0;
    if (len >= CANDID_MAGIC_SIZE && candid_check_magic(data, len)) {
        offset = CANDID_MAGIC_SIZE;
    }

    return candid_deserialize_principal(data, len, &offset, principal);
}

ic_result_t ic_api_to_wire_text(ic_api_t *api, const char *text) {
    if (api == NULL || text == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_to_wire) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() may only be called once per message");
    }

    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() not allowed for this entry point type");
    }

    api->called_to_wire = true;

    // Reset output buffer
    ic_buffer_clear(&api->output_buffer);

    // Prepend format header
    ic_result_t result =
        ic_buffer_append(&api->output_buffer, CANDID_MAGIC, CANDID_MAGIC_SIZE);
    if (result != IC_OK) {
        return result;
    }

    // Encode text value
    result = candid_serialize_text(&api->output_buffer, text);
    if (result != IC_OK) {
        return result;
    }

    // Dispatch response
    return ic_api_msg_reply(api);
}

ic_result_t ic_api_to_wire_nat(ic_api_t *api, uint64_t value) {
    if (api == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_to_wire) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() may only be called once per message");
    }

    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() not allowed for this entry point type");
    }

    api->called_to_wire = true;

    // Reset output buffer
    ic_buffer_clear(&api->output_buffer);

    // Prepend format header
    ic_result_t result =
        ic_buffer_append(&api->output_buffer, CANDID_MAGIC, CANDID_MAGIC_SIZE);
    if (result != IC_OK) {
        return result;
    }

    // Encode nat value
    result = candid_serialize_nat(&api->output_buffer, value);
    if (result != IC_OK) {
        return result;
    }

    // Dispatch response
    return ic_api_msg_reply(api);
}

ic_result_t ic_api_to_wire_int(ic_api_t *api, int64_t value) {
    if (api == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_to_wire) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() may only be called once per message");
    }

    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() not allowed for this entry point type");
    }

    api->called_to_wire = true;

    // Reset output buffer
    ic_buffer_clear(&api->output_buffer);

    // Prepend format header
    ic_result_t result =
        ic_buffer_append(&api->output_buffer, CANDID_MAGIC, CANDID_MAGIC_SIZE);
    if (result != IC_OK) {
        return result;
    }

    // Encode int value
    result = candid_serialize_int(&api->output_buffer, value);
    if (result != IC_OK) {
        return result;
    }

    // Dispatch response
    return ic_api_msg_reply(api);
}

ic_result_t ic_api_to_wire_blob(ic_api_t *api, const uint8_t *blob,
                                size_t blob_len) {
    if (api == NULL || blob == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_to_wire) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() may only be called once per message");
    }

    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() not allowed for this entry point type");
    }

    api->called_to_wire = true;

    // Reset output buffer
    ic_buffer_clear(&api->output_buffer);

    // Prepend format header
    ic_result_t result =
        ic_buffer_append(&api->output_buffer, CANDID_MAGIC, CANDID_MAGIC_SIZE);
    if (result != IC_OK) {
        return result;
    }

    // Encode blob value
    result = candid_serialize_blob(&api->output_buffer, blob, blob_len);
    if (result != IC_OK) {
        return result;
    }

    // Dispatch response
    return ic_api_msg_reply(api);
}

ic_result_t ic_api_to_wire_principal(ic_api_t *api,
                                     const ic_principal_t *principal) {
    if (api == NULL || principal == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_to_wire) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() may only be called once per message");
    }

    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() not allowed for this entry point type");
    }

    api->called_to_wire = true;

    // Reset output buffer
    ic_buffer_clear(&api->output_buffer);

    // Prepend format header
    ic_result_t result =
        ic_buffer_append(&api->output_buffer, CANDID_MAGIC, CANDID_MAGIC_SIZE);
    if (result != IC_OK) {
        return result;
    }

    // Encode principal value
    result = candid_serialize_principal(&api->output_buffer, principal);
    if (result != IC_OK) {
        return result;
    }

    // Dispatch response
    return ic_api_msg_reply(api);
}

ic_result_t ic_api_to_wire_empty(ic_api_t *api) {
    if (api == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (api->called_to_wire) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() may only be called once per message");
    }

    if (!ic_entry_can_reply(api->entry_type)) {
        ic_api_trap(
            "cdk-c: ic_api_to_wire() not allowed for this entry point type");
    }

    api->called_to_wire = true;

    // Reset output buffer
    ic_buffer_clear(&api->output_buffer);

    // Prepend format header
    ic_result_t result =
        ic_buffer_append(&api->output_buffer, CANDID_MAGIC, CANDID_MAGIC_SIZE);
    if (result != IC_OK) {
        return result;
    }

    // Dispatch empty response
    return ic_api_msg_reply(api);
}

ic_result_t ic_api_msg_reply(ic_api_t *api) {
    if (api == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    const uint8_t *data = ic_buffer_data(&api->output_buffer);
    size_t size = ic_buffer_size(&api->output_buffer);

    // Validate format header
    if (size < CANDID_MAGIC_SIZE || !candid_check_magic(data, size)) {
        ic_api_trap("cdk-c: output buffer missing Candid format header");
    }

    // Transfer data to reply buffer
    if (size > UINT32_MAX) {
        return IC_ERR_BUFFER_OVERFLOW;
    }

    ic0_msg_reply_data_append((uintptr_t)data, (uint32_t)size);

    // Clean up buffers
    ic_buffer_clear(&api->input_buffer);
    ic_buffer_clear(&api->output_buffer);

    // Finalize reply
    ic0_msg_reply();

    return IC_OK;
}
