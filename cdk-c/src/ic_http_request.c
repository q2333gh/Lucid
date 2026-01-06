// HTTP Request API Implementation
#include "ic_http_request.h"
#include "ic0.h"
#include "ic_api.h"
#include "ic_call.h"
#include "ic_candid_builder.h"
#include "idl/candid.h"
#include "idl/leb128.h"
#include <stdlib.h>
#include <string.h>

// =============================================================================
// Cost Calculation
// =============================================================================

ic_result_t ic_http_request_cost(const ic_http_request_args_t *args,
                                 uint64_t                     *cost_high,
                                 uint64_t                     *cost_low) {
    if (!args || !args->url || !cost_high || !cost_low) {
        return IC_ERR_INVALID_ARG;
    }

    // Calculate request size
    // According to IC spec, request size includes:
    // - URL length
    // - Sum of (header_name + header_value) lengths
    // - Body length
    // - Transform context length (if present)
    // - Transform function method name length (if present)

    uint64_t request_size = strlen(args->url);

    // Add headers size
    for (size_t i = 0; i < args->headers_count; i++) {
        if (args->headers[i].name) {
            request_size += strlen(args->headers[i].name);
        }
        if (args->headers[i].value) {
            request_size += strlen(args->headers[i].value);
        }
    }

    // Add body size
    if (args->body && args->body_len > 0) {
        request_size += args->body_len;
    }

    // Add transform size (if present)
    if (args->transform) {
        if (args->transform->function_method) {
            request_size += strlen(args->transform->function_method);
        }
        request_size += args->transform->context_len;
    }

    // Get max response bytes (default: 2MB as per IC spec)
    uint64_t max_res_bytes = args->max_response_bytes;
    if (max_res_bytes == 0) {
        max_res_bytes = 2000000; // 2MB default
    }

    // Call IC0 cost function
    // Result is 128-bit cycles count in little-endian format
    uint8_t cost_bytes[16];
    ic0_cost_http_request((int64_t)request_size, (int64_t)max_res_bytes,
                          (uintptr_t)cost_bytes);

    // Extract 128-bit cost (little-endian)
    *cost_low =
        ((uint64_t)cost_bytes[0]) | ((uint64_t)cost_bytes[1] << 8) |
        ((uint64_t)cost_bytes[2] << 16) | ((uint64_t)cost_bytes[3] << 24) |
        ((uint64_t)cost_bytes[4] << 32) | ((uint64_t)cost_bytes[5] << 40) |
        ((uint64_t)cost_bytes[6] << 48) | ((uint64_t)cost_bytes[7] << 56);

    *cost_high =
        ((uint64_t)cost_bytes[8]) | ((uint64_t)cost_bytes[9] << 8) |
        ((uint64_t)cost_bytes[10] << 16) | ((uint64_t)cost_bytes[11] << 24) |
        ((uint64_t)cost_bytes[12] << 32) | ((uint64_t)cost_bytes[13] << 40) |
        ((uint64_t)cost_bytes[14] << 48) | ((uint64_t)cost_bytes[15] << 56);

    return IC_OK;
}

// =============================================================================
// Candid Serialization Helpers
// =============================================================================

// Build HTTP method as Candid variant
static idl_type *build_http_method_type(idl_arena *arena) {
    // variant { get; post; head }
    idl_field fields[3];

    fields[0] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("get"), .name = "get"},
        .type = idl_type_null(arena)
    };

    fields[1] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("post"),
                  .name = "post"},
        .type = idl_type_null(arena)
    };

    fields[2] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("head"),
                  .name = "head"},
        .type = idl_type_null(arena)
    };

    // Sort fields by hash
    idl_fields_sort_inplace(fields, 3);
    return idl_type_variant(arena, fields, 3);
}

static idl_value *build_http_method_value(idl_arena       *arena,
                                          ic_http_method_t method) {
    const char *method_name;
    uint64_t    method_index;

    // Variant fields MUST be in alphabetical order: get=0, head=1, post=2
    switch (method) {
    case IC_HTTP_METHOD_GET:
        method_name = "get";
        method_index = 0;
        break;
    case IC_HTTP_METHOD_HEAD:
        method_name = "head";
        method_index = 1;
        break;
    case IC_HTTP_METHOD_POST:
        method_name = "post";
        method_index = 2;
        break;
    default:
        method_name = "get";
        method_index = 0;
    }

    idl_value_field field = {
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash(method_name),
                  .name = method_name},
        .value = idl_value_null(arena)
    };

    return idl_value_variant(arena, method_index, &field);
}

// Build HTTP header as Candid record
static idl_type *build_http_header_type(idl_arena *arena) {
    // record { name: text; value: text }
    idl_field fields[2];

    fields[0] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("name"),
                  .name = "name"},
        .type = idl_type_text(arena)
    };

    fields[1] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("value"),
                  .name = "value"},
        .type = idl_type_text(arena)
    };

    idl_fields_sort_inplace(fields, 2);
    return idl_type_record(arena, fields, 2);
}

static idl_value *build_http_header_value(idl_arena              *arena,
                                          const ic_http_header_t *header) {
    idl_value_field fields[2];

    fields[0] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("name"),
                  .name = "name"},
        .value = idl_value_text_cstr(arena, header->name)
    };

    fields[1] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("value"),
                  .name = "value"},
        .value = idl_value_text_cstr(arena, header->value)
    };

    idl_value_fields_sort_inplace(fields, 2);
    return idl_value_record(arena, fields, 2);
}

// Build transform context type and value
static idl_type *build_transform_context_type(idl_arena *arena) {
    // record {
    //   function: func(record { response: http_request_result; context: blob })
    //   -> (http_request_result) query; context: blob;
    // }
    idl_field fields[2];

    // For simplicity, we use idl_type_reserved for the function type
    // The actual function type is complex and handled by the runtime
    fields[0] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("function"),
                  .name = "function"},
        .type = idl_type_reserved(arena)
    };

    // blob is vec nat8
    fields[1] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("context"),
                  .name = "context"},
        .type = idl_type_vec(arena, idl_type_nat8(arena))
    };

    idl_fields_sort_inplace(fields, 2);
    return idl_type_record(arena, fields, 2);
}

static idl_value *
build_transform_context_value(idl_arena                    *arena,
                              const ic_transform_context_t *transform) {
    idl_value_field fields[2];

    // Build function reference (Candid func type)
    // func = record { method: text; principal: principal }
    idl_value_field func_fields[2];

    func_fields[0] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("method"),
                  .name = "method"},
        .value = idl_value_text_cstr(arena, transform->function_method)
    };

    func_fields[1] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("principal"),
                  .name = "principal"},
        .value = idl_value_principal(arena, transform->function_principal.bytes,
                                     transform->function_principal.len)
    };

    idl_value_fields_sort_inplace(func_fields, 2);
    idl_value *func_value = idl_value_record(arena, func_fields, 2);

    fields[0] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("function"),
                  .name = "function"},
        .value = func_value
    };

    fields[1] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("context"),
                  .name = "context"},
        .value =
            idl_value_blob(arena, transform->context, transform->context_len)
    };

    idl_value_fields_sort_inplace(fields, 2);
    return idl_value_record(arena, fields, 2);
}

// Build complete HttpRequestArgs
static ic_result_t
build_http_request_args_candid(idl_arena                    *arena,
                               const ic_http_request_args_t *args,
                               idl_value                   **value_out) {
    // HttpRequestArgs = record {
    //   url: text;
    //   max_response_bytes: opt nat64;
    //   method: variant { get; post; head };
    //   headers: vec record { name: text; value: text };
    //   body: opt blob;
    //   transform: opt record { function: func; context: blob };
    //   is_replicated: opt bool;
    // }

    idl_value_field fields[7];
    size_t          field_idx = 0;

    // 1. url: text (required)
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("url"), .name = "url"},
        .value = idl_value_text_cstr(arena, args->url)
    };

    // 2. max_response_bytes: opt nat64
    idl_value *max_bytes_val;
    if (args->max_response_bytes > 0) {
        max_bytes_val = idl_value_opt_some(
            arena, idl_value_nat64(arena, args->max_response_bytes));
    } else {
        max_bytes_val = idl_value_opt_none(arena);
    }
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("max_response_bytes"),
                  .name = "max_response_bytes"},
        .value = max_bytes_val
    };

    // 3. method: variant { get; post; head }
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("method"),
                  .name = "method"},
        .value = build_http_method_value(arena, args->method)
    };

    // 4. headers: vec record { name: text; value: text }
    idl_value **header_values = NULL;
    if (args->headers_count > 0) {
        header_values =
            idl_arena_alloc(arena, sizeof(idl_value *) * args->headers_count);
        for (size_t i = 0; i < args->headers_count; i++) {
            header_values[i] =
                build_http_header_value(arena, &args->headers[i]);
        }
    }
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("headers"),
                  .name = "headers"},
        .value = idl_value_vec(arena, header_values, args->headers_count)
    };

    // 5. body: opt blob
    idl_value *body_val;
    if (args->body && args->body_len > 0) {
        body_val = idl_value_opt_some(
            arena, idl_value_blob(arena, args->body, args->body_len));
    } else {
        body_val = idl_value_opt_none(arena);
    }
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("body"),
                  .name = "body"},
        .value = body_val
    };

    // 6. transform: opt record { function: func; context: blob }
    idl_value *transform_val;
    if (args->transform) {
        transform_val = idl_value_opt_some(
            arena, build_transform_context_value(arena, args->transform));
    } else {
        transform_val = idl_value_opt_none(arena);
    }
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("transform"),
                  .name = "transform"},
        .value = transform_val
    };

    // 7. is_replicated: opt bool
    // Note: Default should be None (not Some(false))
    // Only set to Some if explicitly configured
    idl_value *replicated_val = idl_value_opt_none(arena);
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("is_replicated"),
                  .name = "is_replicated"},
        .value = replicated_val
    };

    // Sort fields and create record
    idl_value_fields_sort_inplace(fields, field_idx);
    *value_out = idl_value_record(arena, fields, field_idx);

    return IC_OK;
}

// =============================================================================
// Async HTTP Request
// =============================================================================

ic_result_t ic_http_request_async(const ic_http_request_args_t *args,
                                  void (*reply_callback)(void *env),
                                  void (*reject_callback)(void *env),
                                  void *user_data) {
    if (!args || !args->url) {
        return IC_ERR_INVALID_ARG;
    }

    // 1. Calculate required cycles
    uint64_t    cost_high, cost_low;
    ic_result_t cost_res = ic_http_request_cost(args, &cost_high, &cost_low);
    if (cost_res != IC_OK) {
        return cost_res;
    }

    // 2. Get management canister principal
    ic_principal_t mgmt_canister;
    if (ic_principal_management_canister(&mgmt_canister) != IC_OK) {
        return IC_ERR_INVALID_STATE;
    }

    // 3. Build Candid arguments
    idl_arena arena;
    idl_arena_init(&arena, 8192); // Larger initial size for HTTP requests

    idl_value *arg_value;
    if (build_http_request_args_candid(&arena, args, &arg_value) != IC_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    // 4. Serialize arguments
    idl_builder builder;
    idl_builder_init(&builder, &arena);

    // Build complete type manually (avoids variant inference limitations)
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

    // 5. Create inter-canister call
    ic_call_t *call = ic_call_new(&mgmt_canister, "http_request");
    if (!call) {
        idl_arena_destroy(&arena);
        return IC_ERR_OUT_OF_MEMORY;
    }

    // 6. Set arguments and cycles
    ic_call_with_arg(call, candid_bytes, candid_len);
    ic_call_with_cycles128(call, cost_high, cost_low);

    // 7. Set callbacks
    if (reply_callback) {
        ic_call_on_reply(call, reply_callback, user_data);
    }
    if (reject_callback) {
        ic_call_on_reject(call, reject_callback, user_data);
    }

    // 8. Perform call
    ic_result_t call_res = ic_call_perform(call);

    // Cleanup
    ic_call_free(call);
    idl_arena_destroy(&arena);

    return call_res;
}

// =============================================================================
// Response Parsing
// =============================================================================

/**
 * Parse a single HTTP header from Candid record value
 * Returns IC_OK on success, IC_ERR_INVALID_STATE on failure
 */
static ic_result_t parse_http_header(idl_value        *header_val,
                                     ic_http_header_t *out) {
    if (!header_val || !out || header_val->kind != IDL_VALUE_RECORD) {
        return IC_ERR_INVALID_STATE;
    }

    out->name = NULL;
    out->value = NULL;

    uint32_t name_hash = idl_hash("name");
    uint32_t value_hash = idl_hash("value");

    for (size_t i = 0; i < header_val->data.record.len; i++) {
        idl_value_field *hfield = &header_val->data.record.fields[i];
        uint32_t         field_hash = hfield->label.id;

        if (field_hash == name_hash && hfield->value->kind == IDL_VALUE_TEXT) {
            size_t name_len = hfield->value->data.text.len;
            out->name = (char *)malloc(name_len + 1);
            if (out->name) {
                memcpy(out->name, hfield->value->data.text.data, name_len);
                out->name[name_len] = '\0';
            }
        } else if (field_hash == value_hash &&
                   hfield->value->kind == IDL_VALUE_TEXT) {
            size_t value_len = hfield->value->data.text.len;
            out->value = (char *)malloc(value_len + 1);
            if (out->value) {
                memcpy(out->value, hfield->value->data.text.data, value_len);
                out->value[value_len] = '\0';
            }
        }
    }

    return IC_OK;
}

/**
 * Extract NAT value from idl_value (handles different nat types including
 * bignum)
 */
static ic_result_t extract_nat_value(const idl_value *value, uint64_t *out) {
    if (!value || !out) {
        return IC_ERR_INVALID_ARG;
    }

    // IMPORTANT: Check NAT (bignum) first as it's the common case for HTTP
    // status
    if (value->kind == IDL_VALUE_NAT) {
        size_t   consumed;
        uint64_t decoded_val;
        if (idl_uleb128_decode(value->data.bignum.data, value->data.bignum.len,
                               &consumed, &decoded_val) == IDL_STATUS_OK) {
            *out = decoded_val;
            return IC_OK;
        }
        return IC_ERR_INVALID_STATE;
    } else if (value->kind == IDL_VALUE_NAT64) {
        *out = value->data.nat64_val;
        return IC_OK;
    } else if (value->kind == IDL_VALUE_NAT32) {
        *out = value->data.nat32_val;
        return IC_OK;
    } else if (value->kind == IDL_VALUE_NAT16) {
        *out = value->data.nat16_val;
        return IC_OK;
    } else if (value->kind == IDL_VALUE_NAT8) {
        *out = value->data.nat8_val;
        return IC_OK;
    }

    return IC_ERR_INVALID_STATE;
}

/**
 * Parse headers field from Candid value
 */
static ic_result_t parse_headers_field(idl_value                *vec_value,
                                       ic_http_request_result_t *result) {
    if (vec_value->kind != IDL_VALUE_VEC) {
        return IC_ERR_INVALID_STATE;
    }

    result->headers_count = vec_value->data.vec.len;
    if (result->headers_count == 0) {
        return IC_OK;
    }

    result->headers = (ic_http_header_t *)malloc(sizeof(ic_http_header_t) *
                                                 result->headers_count);
    if (!result->headers) {
        return IC_ERR_OUT_OF_MEMORY;
    }

    for (size_t j = 0; j < result->headers_count; j++) {
        parse_http_header(vec_value->data.vec.items[j], &result->headers[j]);
    }

    return IC_OK;
}

/**
 * Parse body field from Candid value
 */
static ic_result_t parse_body_field(idl_value                *body_value,
                                    ic_http_request_result_t *result) {
    if (body_value->kind == IDL_VALUE_BLOB) {
        result->body_len = body_value->data.blob.len;
        if (result->body_len == 0) {
            return IC_OK;
        }

        result->body = (uint8_t *)malloc(result->body_len);
        if (!result->body) {
            return IC_ERR_OUT_OF_MEMORY;
        }
        memcpy(result->body, body_value->data.blob.data, result->body_len);
        return IC_OK;
    }

    if (body_value->kind == IDL_VALUE_VEC) {
        result->body_len = body_value->data.vec.len;
        if (result->body_len == 0) {
            return IC_OK;
        }

        result->body = (uint8_t *)malloc(result->body_len);
        if (!result->body) {
            return IC_ERR_OUT_OF_MEMORY;
        }

        for (size_t j = 0; j < result->body_len; j++) {
            if (body_value->data.vec.items[j]->kind == IDL_VALUE_NAT8) {
                result->body[j] = body_value->data.vec.items[j]->data.nat8_val;
            }
        }
        return IC_OK;
    }

    return IC_ERR_INVALID_STATE;
}

/**
 * Parse HTTP response from Candid-encoded reply
 *
 * Deserializes the HttpRequestResult from management canister's response.
 * HttpRequestResult = record {
 *   status: nat;
 *   headers: vec record { name: text; value: text };
 *   body: blob;
 * }
 */
ic_result_t ic_http_parse_response(const uint8_t            *candid_data,
                                   size_t                    candid_len,
                                   ic_http_request_result_t *result) {
    if (!candid_data || candid_len == 0 || !result) {
        return IC_ERR_INVALID_ARG;
    }

    // Initialize result
    result->status = 0;
    result->headers = NULL;
    result->headers_count = 0;
    result->body = NULL;
    result->body_len = 0;

    // Create arena for deserialization
    idl_arena arena;
    idl_arena_init(&arena, 8192);

    // Create deserializer
    idl_deserializer *de = NULL;
    if (idl_deserializer_new(candid_data, candid_len, &arena, &de) !=
        IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    // Get the value (should be a record)
    idl_type  *type;
    idl_value *val;
    if (idl_deserializer_get_value(de, &type, &val) != IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    if (!val || val->kind != IDL_VALUE_RECORD) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    // Parse record fields
    if (val->data.record.len == 0) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    // Field hashes for HttpRequestResult
    uint32_t status_hash = idl_hash("status");
    uint32_t headers_hash = idl_hash("headers");
    uint32_t body_hash = idl_hash("body");

    for (size_t i = 0; i < val->data.record.len; i++) {
        idl_value_field *field = &val->data.record.fields[i];
        uint32_t         field_hash = field->label.id;

        if (field_hash == status_hash) {
            // Extract status (nat)
            extract_nat_value(field->value, &result->status);
        } else if (field_hash == headers_hash) {
            ic_result_t res = parse_headers_field(field->value, result);
            if (res != IC_OK) {
                ic_http_free_result(result);
                idl_arena_destroy(&arena);
                return res;
            }
        } else if (field_hash == body_hash) {
            ic_result_t res = parse_body_field(field->value, result);
            if (res != IC_OK) {
                ic_http_free_result(result);
                idl_arena_destroy(&arena);
                return res;
            }
        }
    }

    idl_arena_destroy(&arena);
    return IC_OK;
}

/**
 * Free resources allocated in HTTP request result
 */
void ic_http_free_result(ic_http_request_result_t *result) {
    if (!result) {
        return;
    }

    // Free headers
    if (result->headers) {
        for (size_t i = 0; i < result->headers_count; i++) {
            if (result->headers[i].name) {
                free(result->headers[i].name);
            }
            if (result->headers[i].value) {
                free(result->headers[i].value);
            }
        }
        free(result->headers);
        result->headers = NULL;
    }

    // Free body
    if (result->body) {
        free(result->body);
        result->body = NULL;
    }

    result->status = 0;
    result->headers_count = 0;
    result->body_len = 0;
}

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Format HTTP response body preview (printable chars only)
 */
size_t ic_http_format_body_preview(const uint8_t *body,
                                   size_t         body_len,
                                   char          *buffer,
                                   size_t         buffer_size) {
    if (!body || body_len == 0 || !buffer || buffer_size == 0) {
        return 0;
    }

    size_t preview_len = body_len < 500 ? body_len : 500;
    size_t written = 0;

    for (size_t i = 0; i < preview_len && written < buffer_size - 1; i++) {
        if (body[i] >= 32 && body[i] < 127) {
            buffer[written++] = body[i];
        } else if (body[i] == '\n') {
            buffer[written++] = '\n';
        } else {
            buffer[written++] = '.';
        }
    }

    buffer[written] = '\0';
    return written;
}

/**
 * Get and parse HTTP response from callback
 */
ic_result_t
ic_http_get_and_parse_response_from_callback(ic_http_request_result_t *result) {
    if (!result) {
        return IC_ERR_INVALID_ARG;
    }

    // Initialize result
    result->status = 0;
    result->headers = NULL;
    result->headers_count = 0;
    result->body = NULL;
    result->body_len = 0;

    // Get the raw Candid response data size from message arguments
    uint32_t response_size = ic0_msg_arg_data_size();

    if (response_size == 0) {
        return IC_ERR_INVALID_STATE;
    }

    // Allocate buffer for response data
    uint8_t *response_data = (uint8_t *)malloc(response_size);
    if (!response_data) {
        return IC_ERR_OUT_OF_MEMORY;
    }

    // Copy response data
    ic0_msg_arg_data_copy((uintptr_t)response_data, 0, response_size);

    // Parse HTTP response
    ic_result_t parse_res =
        ic_http_parse_response(response_data, response_size, result);

    free(response_data);

    return parse_res;
}

/**
 * Get reject information from callback
 */
ic_result_t ic_http_get_reject_info(ic_http_reject_info_t *info) {
    if (!info) {
        return IC_ERR_INVALID_ARG;
    }

    // Initialize info
    info->code = 0;
    info->message = NULL;
    info->message_len = 0;

    // Get reject code
    info->code = ic_api_msg_reject_code();

    // Get reject message
    // First, try to get message with a reasonable buffer size
    char        temp_buf[512];
    size_t      reject_len = 0;
    ic_result_t msg_result =
        ic_api_msg_reject_message(temp_buf, sizeof(temp_buf), &reject_len);

    if (msg_result == IC_OK && reject_len > 0) {
        // Allocate memory for message
        info->message = (char *)malloc(reject_len + 1);
        if (!info->message) {
            return IC_ERR_OUT_OF_MEMORY;
        }

        // Copy message
        memcpy(info->message, temp_buf, reject_len);
        info->message[reject_len] = '\0';
        info->message_len = reject_len;
    }

    return IC_OK;
}

/**
 * Simplified reply callback wrapper
 */
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

    // Get and parse HTTP response
    ic_http_request_result_t result;
    ic_result_t              parse_res =
        ic_http_get_and_parse_response_from_callback(&result);

    if (parse_res != IC_OK) {
        ic_api_to_wire_text(api, "Failed to parse HTTP response");
        ic_api_free(api);
        return;
    }

    // Call user-provided handler
    handler(api, &result);

    // Cleanup
    ic_http_free_result(&result);
    ic_api_free(api);
}

/**
 * Simplified reject callback wrapper
 */
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

    // Get reject information
    ic_http_reject_info_t reject_info;
    ic_result_t           info_res = ic_http_get_reject_info(&reject_info);

    if (info_res == IC_OK) {
        // Call user-provided handler
        handler(api, &reject_info);
        ic_http_free_reject_info(&reject_info);
    } else {
        ic_api_to_wire_text(api, "HTTP request rejected");
    }

    ic_api_free(api);
}
