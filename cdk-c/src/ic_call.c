#include "ic_call.h"
#include "ic0.h"
#include <stdlib.h>
#include <string.h>

ic_call_t *ic_call_new(const ic_principal_t *callee, const char *method) {
    if (!callee || !method)
        return NULL;

    ic_call_t *call = malloc(sizeof(ic_call_t));
    if (!call)
        return NULL;

    memset(call, 0, sizeof(ic_call_t));

    // Copy callee
    call->callee = *callee;

    // Copy method name
    size_t name_len = strlen(method);
    call->method_name = malloc(name_len + 1);
    if (!call->method_name) {
        free(call);
        return NULL;
    }
    memcpy(call->method_name, method, name_len + 1);
    call->method_name_len = name_len;

    // Init args buffer
    if (ic_buffer_init(&call->args) != IC_OK) {
        free(call->method_name);
        free(call);
        return NULL;
    }

    return call;
}

void ic_call_free(ic_call_t *call) {
    if (!call)
        return;

    if (call->method_name) {
        free(call->method_name);
    }

    ic_buffer_free(&call->args);

    free(call);
}

ic_result_t ic_call_with_arg(ic_call_t *call, const uint8_t *data, size_t len) {
    if (!call)
        return IC_ERR_INVALID_ARG;
    return ic_buffer_append(&call->args, data, len);
}

void ic_call_with_cycles128(ic_call_t *call, uint64_t high, uint64_t low) {
    if (!call)
        return;
    call->cycles_high = high;
    call->cycles_low = low;
}

void ic_call_with_cycles(ic_call_t *call, uint64_t amount) {
    if (!call)
        return;
    call->cycles_high = 0;
    call->cycles_low = amount;
}

void ic_call_on_reply(ic_call_t *call, ic_call_cb_t callback, void *env) {
    if (!call)
        return;
    call->reply_fun = callback;
    call->reply_env = env;
}

void ic_call_on_reject(ic_call_t *call, ic_call_cb_t callback, void *env) {
    if (!call)
        return;
    call->reject_fun = callback;
    call->reject_env = env;
}

ic_result_t ic_call_perform(ic_call_t *call) {
    if (!call)
        return IC_ERR_INVALID_ARG;

    // 1. Start the call
    ic0_call_new((uintptr_t)call->callee.bytes, call->callee.len,
                 (uintptr_t)call->method_name, call->method_name_len,
                 (uintptr_t)call->reply_fun, (uintptr_t)call->reply_env,
                 (uintptr_t)call->reject_fun, (uintptr_t)call->reject_env);

    // 2. Append argument data
    if (!ic_buffer_empty(&call->args)) {
        ic0_call_data_append((uintptr_t)call->args.data, call->args.size);
    }

    // 3. Add cycles if any
    if (call->cycles_low > 0 || call->cycles_high > 0) {
        ic0_call_cycles_add128((int64_t)call->cycles_high,
                               (int64_t)call->cycles_low);
    }

    // 4. Perform the call
    uint32_t err = ic0_call_perform();

    if (err != 0) {
        return IC_ERR; // Map error code if needed
    }

    return IC_OK;
}
