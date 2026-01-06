// HTTP Request API Implementation
#include "ic_http_request.h"
#include "ic_api.h"
#include "ic_call.h"
#include "ic_candid_builder.h"
#include "ic_principal.h"
#include "idl/candid.h"
#include <stdlib.h>

// Forward declaration for internal function
ic_result_t build_http_request_args_candid(idl_arena                    *arena,
                                           const ic_http_request_args_t *args,
                                           idl_value **value_out);

ic_result_t ic_http_request_async(const ic_http_request_args_t *args,
                                  void (*reply_callback)(void *env),
                                  void (*reject_callback)(void *env),
                                  void *user_data) {
    if (!args || !args->url) {
        return IC_ERR_INVALID_ARG;
    }

    uint64_t    cost_high;
    uint64_t    cost_low;
    ic_result_t cost_res = ic_http_request_cost(args, &cost_high, &cost_low);
    if (cost_res != IC_OK) {
        return cost_res;
    }

    ic_principal_t mgmt_canister;
    if (ic_principal_management_canister(&mgmt_canister) != IC_OK) {
        return IC_ERR_INVALID_STATE;
    }

    idl_arena arena;
    idl_arena_init(&arena, 8192);

    idl_value *arg_value;
    if (build_http_request_args_candid(&arena, args, &arg_value) != IC_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    idl_builder builder;
    idl_builder_init(&builder, &arena);

    extern idl_type *build_http_request_args_type(idl_arena * arena);
    idl_type        *arg_type = build_http_request_args_type(&arena);
    if (!arg_type) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    idl_status status = idl_builder_arg(&builder, arg_type, arg_value);
    if (status != IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    uint8_t *candid_bytes;
    size_t   candid_len;
    if (idl_builder_serialize(&builder, &candid_bytes, &candid_len) !=
        IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    ic_call_t *call = ic_call_new(&mgmt_canister, "http_request");
    if (!call) {
        idl_arena_destroy(&arena);
        return IC_ERR_OUT_OF_MEMORY;
    }

    ic_call_with_arg(call, candid_bytes, candid_len);
    ic_call_with_cycles128(call, cost_high, cost_low);

    if (reply_callback) {
        ic_call_on_reply(call, reply_callback, user_data);
    }
    if (reject_callback) {
        ic_call_on_reject(call, reject_callback, user_data);
    }

    ic_result_t call_res = ic_call_perform(call);

    ic_call_free(call);
    idl_arena_destroy(&arena);

    return call_res;
}

void ic_http_reply_callback_wrapper(void *env) {
    ic_http_reply_handler_t handler = (ic_http_reply_handler_t)env;
    if (!handler) {
        ic_api_trap("HTTP reply callback: handler is NULL");
        return;
    }

    ic_api_t *api =
        ic_api_init(IC_ENTRY_REPLY_CALLBACK, "ic_http_reply", false);
    if (!api) {
        ic_api_trap("Failed to initialize API in HTTP reply callback");
        return;
    }

    ic_http_request_result_t result;
    ic_result_t              parse_res =
        ic_http_get_and_parse_response_from_callback(&result);

    if (parse_res != IC_OK) {
        ic_api_to_wire_text(api, "Failed to parse HTTP response");
        ic_api_free(api);
        return;
    }

    handler(api, &result);

    ic_http_free_result(&result);
    ic_api_free(api);
}

void ic_http_reject_callback_wrapper(void *env) {
    ic_http_reject_handler_t handler = (ic_http_reject_handler_t)env;
    if (!handler) {
        ic_api_trap("HTTP reject callback: handler is NULL");
        return;
    }

    ic_api_t *api =
        ic_api_init(IC_ENTRY_REJECT_CALLBACK, "ic_http_reject", false);
    if (!api) {
        ic_api_trap("Failed to initialize API in HTTP reject callback");
        return;
    }

    ic_http_reject_info_t reject_info;
    ic_result_t           info_res = ic_http_get_reject_info(&reject_info);

    if (info_res == IC_OK) {
        handler(api, &reject_info);
        ic_http_free_reject_info(&reject_info);
    } else {
        ic_api_to_wire_text(api, "HTTP request rejected");
    }

    ic_api_free(api);
}
