// Native IC0 mock helpers for running canister code on Linux.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef __wasm__

typedef enum {
    IC0_MOCK_CALL_REPLY = 0,
    IC0_MOCK_CALL_REJECT = 1,
} ic0_mock_call_action_t;

typedef struct {
    const uint8_t *callee;
    size_t         callee_len;
    const char    *method;
    size_t         method_len;
    const uint8_t *arg_data;
    size_t         arg_len;
    uint64_t       cycles_high;
    uint64_t       cycles_low;
} ic0_mock_call_t;

typedef struct {
    ic0_mock_call_action_t action;
    const uint8_t         *data;
    size_t                 len;
    // If true, mock will free data after copying.
    bool        data_owned;
    uint32_t    reject_code;
    const char *reject_msg;
    size_t      reject_msg_len;
    // If true, mock will free reject_msg after copying.
    bool reject_msg_owned;
} ic0_mock_call_response_t;

typedef int (*ic0_mock_call_handler_t)(const ic0_mock_call_t    *call,
                                       ic0_mock_call_response_t *out);

void ic0_mock_reset(void);
void ic0_mock_clear_reply(void);
void ic0_mock_set_arg_data(const uint8_t *data, size_t len);
void ic0_mock_set_caller(const uint8_t *data, size_t len);
void ic0_mock_set_self(const uint8_t *data, size_t len);
void ic0_mock_take_reply(uint8_t **out_data, size_t *out_len);

void    ic0_mock_set_time(int64_t time_ns);
int64_t ic0_mock_get_time(void);

void ic0_mock_stable_reset(void);

void ic0_mock_set_call_handler(ic0_mock_call_handler_t handler);

#endif
