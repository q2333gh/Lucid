/*
 * IC0 API Native Mock Implementation
 *
 * Provides native implementations of IC0 system API functions for running
 * canister code on Linux. Defaults to safe no-ops until configured via
 * ic0_mock helpers.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ic0.h"
#include "ic0_mock.h"
#include "ic_call.h"
#include "ic_principal.h"
#include "ic_storage.h"

#ifndef __wasm__

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} mock_buffer_t;

typedef struct {
    uint8_t      *callee;
    size_t        callee_len;
    char         *method;
    size_t        method_len;
    mock_buffer_t args;
    uint64_t      cycles_high;
    uint64_t      cycles_low;
    uintptr_t     reply_fun;
    uintptr_t     reply_env;
    uintptr_t     reject_fun;
    uintptr_t     reject_env;
} mock_call_state_t;

typedef struct {
    uint8_t *arg_data;
    size_t   arg_len;

    uint8_t caller[IC_PRINCIPAL_MAX_LEN];
    size_t  caller_len;
    uint8_t self[IC_PRINCIPAL_MAX_LEN];
    size_t  self_len;

    mock_buffer_t reply;
    bool          replied;

    uint32_t reject_code;
    char    *reject_msg;
    size_t   reject_msg_len;
    bool     rejected;

    int64_t time_ns;
    int64_t global_timer_ns;

    mock_call_state_t       call;
    ic0_mock_call_handler_t call_handler;

    uint8_t *stable;
    size_t   stable_pages;
    size_t   stable_cap_pages;
} mock_ctx_t;

static mock_ctx_t g_mock;

static void mock_buffer_reset(mock_buffer_t *buf) {
    if (buf == NULL) {
        return;
    }
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

static int mock_buffer_reserve(mock_buffer_t *buf, size_t needed) {
    if (buf == NULL) {
        return 0;
    }
    if (needed <= buf->cap) {
        return 1;
    }
    size_t new_cap = buf->cap == 0 ? 256 : buf->cap;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    uint8_t *next = realloc(buf->data, new_cap);
    if (next == NULL) {
        return 0;
    }
    buf->data = next;
    buf->cap = new_cap;
    return 1;
}

static int
mock_buffer_append(mock_buffer_t *buf, const uint8_t *data, size_t len) {
    if (buf == NULL) {
        return 0;
    }
    if (len == 0) {
        return 1;
    }
    if (!mock_buffer_reserve(buf, buf->len + len)) {
        return 0;
    }
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    return 1;
}

static void mock_call_reset(mock_call_state_t *call) {
    if (call == NULL) {
        return;
    }
    free(call->callee);
    free(call->method);
    mock_buffer_reset(&call->args);
    memset(call, 0, sizeof(*call));
}

void ic0_mock_reset(void) {
    free(g_mock.arg_data);
    g_mock.arg_data = NULL;
    g_mock.arg_len = 0;
    g_mock.caller_len = 0;
    g_mock.self_len = 0;
    g_mock.reject_code = 0;
    free(g_mock.reject_msg);
    g_mock.reject_msg = NULL;
    g_mock.reject_msg_len = 0;
    g_mock.rejected = false;
    ic0_mock_clear_reply();
    mock_call_reset(&g_mock.call);
}

void ic0_mock_clear_reply(void) {
    mock_buffer_reset(&g_mock.reply);
    g_mock.replied = false;
}

void ic0_mock_set_arg_data(const uint8_t *data, size_t len) {
    free(g_mock.arg_data);
    g_mock.arg_data = NULL;
    g_mock.arg_len = 0;
    if (data == NULL || len == 0) {
        return;
    }
    g_mock.arg_data = malloc(len);
    if (g_mock.arg_data == NULL) {
        return;
    }
    memcpy(g_mock.arg_data, data, len);
    g_mock.arg_len = len;
}

void ic0_mock_set_caller(const uint8_t *data, size_t len) {
    if (data == NULL || len > IC_PRINCIPAL_MAX_LEN) {
        g_mock.caller_len = 0;
        return;
    }
    memcpy(g_mock.caller, data, len);
    g_mock.caller_len = len;
}

void ic0_mock_set_self(const uint8_t *data, size_t len) {
    if (data == NULL || len > IC_PRINCIPAL_MAX_LEN) {
        g_mock.self_len = 0;
        return;
    }
    memcpy(g_mock.self, data, len);
    g_mock.self_len = len;
}

void ic0_mock_take_reply(uint8_t **out_data, size_t *out_len) {
    if (out_data == NULL || out_len == NULL) {
        return;
    }
    if (g_mock.reply.len == 0) {
        *out_data = NULL;
        *out_len = 0;
        return;
    }
    uint8_t *copy = malloc(g_mock.reply.len);
    if (copy == NULL) {
        *out_data = NULL;
        *out_len = 0;
        return;
    }
    memcpy(copy, g_mock.reply.data, g_mock.reply.len);
    *out_data = copy;
    *out_len = g_mock.reply.len;
    ic0_mock_clear_reply();
}

void ic0_mock_set_time(int64_t time_ns) { g_mock.time_ns = time_ns; }

int64_t ic0_mock_get_time(void) { return g_mock.time_ns; }

void ic0_mock_stable_reset(void) {
    free(g_mock.stable);
    g_mock.stable = NULL;
    g_mock.stable_pages = 0;
    g_mock.stable_cap_pages = 0;
}

void ic0_mock_set_call_handler(ic0_mock_call_handler_t handler) {
    g_mock.call_handler = handler;
}

// Stub implementations return safe defaults
uint32_t ic0_msg_arg_data_size(void) { return (uint32_t)g_mock.arg_len; }

void ic0_msg_arg_data_copy(uintptr_t dst, uint32_t off, uint32_t size) {
    if (g_mock.arg_data == NULL || off + size > g_mock.arg_len) {
        return;
    }
    memcpy((void *)dst, g_mock.arg_data + off, size);
}

uint32_t ic0_msg_caller_size(void) { return (uint32_t)g_mock.caller_len; }

void ic0_msg_caller_copy(uintptr_t dst, uint32_t off, uint32_t size) {
    if (off + size > g_mock.caller_len) {
        return;
    }
    memcpy((void *)dst, g_mock.caller + off, size);
}

uint32_t ic0_msg_reject_code(void) { return g_mock.reject_code; }

uint32_t ic0_msg_reject_msg_size(void) {
    return (uint32_t)g_mock.reject_msg_len;
}

void ic0_msg_reject_msg_copy(uintptr_t dst, uint32_t off, uint32_t size) {
    if (g_mock.reject_msg == NULL || off + size > g_mock.reject_msg_len) {
        return;
    }
    memcpy((void *)dst, g_mock.reject_msg + off, size);
}

int64_t ic0_msg_deadline(void) { return 0; }

void ic0_msg_reply_data_append(uintptr_t src, uint32_t size) {
    if (size == 0) {
        return;
    }
    mock_buffer_append(&g_mock.reply, (const uint8_t *)src, size);
}

void ic0_msg_reply(void) { g_mock.replied = true; }

void ic0_msg_reject(uintptr_t src, uint32_t size) {
    free(g_mock.reject_msg);
    g_mock.reject_msg = NULL;
    g_mock.reject_msg_len = 0;
    g_mock.reject_code = 5;
    g_mock.rejected = true;
    if (size == 0) {
        return;
    }
    g_mock.reject_msg = malloc(size + 1);
    if (g_mock.reject_msg == NULL) {
        g_mock.reject_msg_len = 0;
        return;
    }
    memcpy(g_mock.reject_msg, (const void *)src, size);
    g_mock.reject_msg[size] = '\0';
    g_mock.reject_msg_len = size;
}

void ic0_msg_cycles_available128(uintptr_t dst) { memset((void *)dst, 0, 16); }

void ic0_msg_cycles_refunded128(uintptr_t dst) { memset((void *)dst, 0, 16); }

void ic0_msg_cycles_accept128(int64_t   max_amount_high,
                              int64_t   max_amount_low,
                              uintptr_t dst) {
    (void)max_amount_high;
    (void)max_amount_low;
    memset((void *)dst, 0, 16);
}

uint32_t ic0_canister_self_size(void) { return (uint32_t)g_mock.self_len; }

void ic0_canister_self_copy(uintptr_t dst, uint32_t off, uint32_t size) {
    if (off + size > g_mock.self_len) {
        return;
    }
    memcpy((void *)dst, g_mock.self + off, size);
}

void ic0_canister_cycle_balance128(uintptr_t dst) {
    memset((void *)dst, 0, 16);
}

uint32_t ic0_canister_status(void) { return 0; }

int64_t ic0_canister_version(void) { return 0; }

void ic0_call_new(uintptr_t callee_src,
                  uint32_t  callee_size,
                  uintptr_t name_src,
                  uint32_t  name_size,
                  uintptr_t reply_fun,
                  uintptr_t reply_env,
                  uintptr_t reject_fun,
                  uintptr_t reject_env) {
    mock_call_reset(&g_mock.call);

    if (callee_size > 0) {
        g_mock.call.callee = malloc(callee_size);
        if (g_mock.call.callee != NULL) {
            memcpy(g_mock.call.callee, (const void *)callee_src, callee_size);
            g_mock.call.callee_len = callee_size;
        }
    }

    if (name_size > 0) {
        g_mock.call.method = malloc(name_size + 1);
        if (g_mock.call.method != NULL) {
            memcpy(g_mock.call.method, (const void *)name_src, name_size);
            g_mock.call.method[name_size] = '\0';
            g_mock.call.method_len = name_size;
        }
    }

    g_mock.call.reply_fun = reply_fun;
    g_mock.call.reply_env = reply_env;
    g_mock.call.reject_fun = reject_fun;
    g_mock.call.reject_env = reject_env;
}

void ic0_call_on_cleanup(uintptr_t fun, uintptr_t env) {
    (void)fun;
    (void)env;
}

void ic0_call_data_append(uintptr_t src, uint32_t size) {
    mock_buffer_append(&g_mock.call.args, (const uint8_t *)src, size);
}

void ic0_call_cycles_add128(int64_t amount_high, int64_t amount_low) {
    g_mock.call.cycles_high = (uint64_t)amount_high;
    g_mock.call.cycles_low = (uint64_t)amount_low;
}

uint32_t ic0_call_perform(void) {
    if (g_mock.call.method == NULL) {
        return 1;
    }

    ic0_mock_call_t call = {
        .callee = g_mock.call.callee,
        .callee_len = g_mock.call.callee_len,
        .method = g_mock.call.method,
        .method_len = g_mock.call.method_len,
        .arg_data = g_mock.call.args.data,
        .arg_len = g_mock.call.args.len,
        .cycles_high = g_mock.call.cycles_high,
        .cycles_low = g_mock.call.cycles_low,
    };

    ic0_mock_call_response_t response;
    memset(&response, 0, sizeof(response));
    response.action = IC0_MOCK_CALL_REJECT;
    response.reject_code = 5;
    response.reject_msg = "mock call handler not set";
    response.reject_msg_len = strlen(response.reject_msg);
    response.data_owned = false;
    response.reject_msg_owned = false;

    if (g_mock.call_handler != NULL) {
        if (g_mock.call_handler(&call, &response) != 0) {
            response.action = IC0_MOCK_CALL_REJECT;
            response.reject_code = 5;
            response.reject_msg = "mock call handler error";
            response.reject_msg_len = strlen(response.reject_msg);
        }
    }

    uint8_t *prev_arg = g_mock.arg_data;
    size_t   prev_arg_len = g_mock.arg_len;
    uint32_t prev_reject_code = g_mock.reject_code;
    char    *prev_reject_msg = g_mock.reject_msg;
    size_t   prev_reject_len = g_mock.reject_msg_len;

    uint8_t *reply_copy = NULL;
    size_t   reply_len = 0;
    char    *reject_copy = NULL;

    if (response.action == IC0_MOCK_CALL_REPLY) {
        reply_len = response.len;
        if (response.len > 0 && response.data != NULL) {
            reply_copy = malloc(response.len);
            if (reply_copy != NULL) {
                memcpy(reply_copy, response.data, response.len);
            } else {
                reply_len = 0;
            }
        }
        if (response.data_owned && response.data != NULL) {
            free((void *)response.data);
        }
        g_mock.arg_data = reply_copy;
        g_mock.arg_len = reply_len;

        if (g_mock.call.reply_fun != 0) {
            ic_call_cb_t cb = (ic_call_cb_t)g_mock.call.reply_fun;
            cb((void *)g_mock.call.reply_env);
        }
    } else {
        g_mock.reject_code = response.reject_code;
        g_mock.reject_msg_len = response.reject_msg_len;
        if (response.reject_msg_len > 0 && response.reject_msg != NULL) {
            reject_copy = malloc(response.reject_msg_len + 1);
            if (reject_copy != NULL) {
                memcpy(reject_copy, response.reject_msg,
                       response.reject_msg_len);
                reject_copy[response.reject_msg_len] = '\0';
            } else {
                g_mock.reject_msg_len = 0;
            }
        }
        g_mock.reject_msg = reject_copy;
        if (response.reject_msg_owned && response.reject_msg != NULL) {
            free((void *)response.reject_msg);
        }

        if (g_mock.call.reject_fun != 0) {
            ic_call_cb_t cb = (ic_call_cb_t)g_mock.call.reject_fun;
            cb((void *)g_mock.call.reject_env);
        }
    }

    free(reply_copy);
    free(reject_copy);

    g_mock.arg_data = prev_arg;
    g_mock.arg_len = prev_arg_len;
    g_mock.reject_code = prev_reject_code;
    g_mock.reject_msg = prev_reject_msg;
    g_mock.reject_msg_len = prev_reject_len;

    mock_call_reset(&g_mock.call);
    return 0;
}

uint32_t ic0_stable_size(void) { return (uint32_t)g_mock.stable_pages; }

uint32_t ic0_stable_grow(uint32_t new_pages) {
    int64_t old_pages = ic0_stable64_grow((int64_t)new_pages);
    if (old_pages < 0 || old_pages > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)old_pages;
}

void ic0_stable_write(uint32_t off, uintptr_t src, uint32_t size) {
    ic0_stable64_write((int64_t)off, (uint64_t)src, (int64_t)size);
}

void ic0_stable_read(uintptr_t dst, uint32_t off, uint32_t size) {
    ic0_stable64_read((uint64_t)dst, (int64_t)off, (int64_t)size);
}

int64_t ic0_stable64_size(void) { return (int64_t)g_mock.stable_pages; }

int64_t ic0_stable64_grow(int64_t new_pages) {
    if (new_pages < 0) {
        return -1;
    }
    size_t old_pages = g_mock.stable_pages;
    size_t next_pages = g_mock.stable_pages + (size_t)new_pages;
    if (next_pages <= g_mock.stable_cap_pages) {
        g_mock.stable_pages = next_pages;
        return (int64_t)old_pages;
    }

    size_t   next_bytes = next_pages * IC_STABLE_PAGE_SIZE_BYTES;
    uint8_t *next = realloc(g_mock.stable, next_bytes);
    if (next == NULL) {
        return -1;
    }
    size_t old_bytes = g_mock.stable_cap_pages * IC_STABLE_PAGE_SIZE_BYTES;
    if (next_bytes > old_bytes) {
        memset(next + old_bytes, 0, next_bytes - old_bytes);
    }
    g_mock.stable = next;
    g_mock.stable_cap_pages = next_pages;
    g_mock.stable_pages = next_pages;
    return (int64_t)old_pages;
}

void ic0_stable64_write(int64_t off, uint64_t src, int64_t size) {
    if (off < 0 || size < 0) {
        return;
    }
    size_t end = (size_t)off + (size_t)size;
    size_t cap = g_mock.stable_pages * IC_STABLE_PAGE_SIZE_BYTES;
    if (end > cap) {
        return;
    }
    memcpy(g_mock.stable + off, (const void *)(uintptr_t)src, (size_t)size);
}

void ic0_stable64_read(uint64_t dst, int64_t off, int64_t size) {
    if (off < 0 || size < 0) {
        return;
    }
    size_t end = (size_t)off + (size_t)size;
    size_t cap = g_mock.stable_pages * IC_STABLE_PAGE_SIZE_BYTES;
    if (end > cap) {
        return;
    }
    memcpy((void *)(uintptr_t)dst, g_mock.stable + off, (size_t)size);
}

int64_t ic0_time(void) { return g_mock.time_ns; }

int64_t ic0_global_timer_set(int64_t timestamp) {
    int64_t prev = g_mock.global_timer_ns;
    g_mock.global_timer_ns = timestamp;
    return prev;
}

uint32_t ic0_is_controller(uintptr_t src, uint32_t size) {
    (void)src;
    (void)size;
    return 0;
}

int32_t ic0_in_replicated_execution(void) { return 0; }

void ic0_cost_http_request(int64_t   request_size,
                           int64_t   max_response_bytes,
                           uintptr_t dst) {
    (void)request_size;
    (void)max_response_bytes;
    memset((void *)dst, 0, 16);
}

void ic0_debug_print(uintptr_t src, uint32_t size) {
    if (src == 0 || size == 0) {
        return;
    }
    fwrite((const void *)src, 1, size, stderr);
    fwrite("\n", 1, 1, stderr);
}

_Noreturn void ic0_trap(uintptr_t src, uint32_t size) {
    if (src != 0 && size != 0) {
        fwrite((const void *)src, 1, size, stderr);
        fwrite("\n", 1, 1, stderr);
    }
    abort();
}

#endif
