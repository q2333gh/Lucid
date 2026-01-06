// HTTP Request Candid Building Helpers
#include "ic_candid_builder.h"
#include "ic_http_request.h"
#include "idl/candid.h"
#include <string.h>

static idl_value *build_http_method_value(idl_arena       *arena,
                                          ic_http_method_t method) {
    const char *method_name;
    uint64_t    method_index;

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

static idl_value *
build_transform_context_value(idl_arena                    *arena,
                              const ic_transform_context_t *transform) {
    idl_value_field fields[2];
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

static idl_value_field build_url_field(idl_arena *arena, const char *url) {
    return (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("url"), .name = "url"},
        .value = idl_value_text_cstr(arena, url)
    };
}

static idl_value_field build_max_response_bytes_field(idl_arena *arena,
                                                      uint64_t   max_bytes) {
    idl_value *max_bytes_val;
    if (max_bytes > 0) {
        max_bytes_val =
            idl_value_opt_some(arena, idl_value_nat64(arena, max_bytes));
    } else {
        max_bytes_val = idl_value_opt_none(arena);
    }
    return (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("max_response_bytes"),
                  .name = "max_response_bytes"},
        .value = max_bytes_val
    };
}

static idl_value_field build_method_field(idl_arena       *arena,
                                          ic_http_method_t method) {
    return (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("method"),
                  .name = "method"},
        .value = build_http_method_value(arena, method)
    };
}

static idl_value_field build_headers_field(idl_arena              *arena,
                                           const ic_http_header_t *headers,
                                           size_t headers_count) {
    idl_value **header_values = NULL;
    if (headers_count > 0) {
        header_values =
            idl_arena_alloc(arena, sizeof(idl_value *) * headers_count);
        for (size_t i = 0; i < headers_count; i++) {
            header_values[i] = build_http_header_value(arena, &headers[i]);
        }
    }
    return (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("headers"),
                  .name = "headers"},
        .value = idl_value_vec(arena, header_values, headers_count)
    };
}

static idl_value_field
build_body_field(idl_arena *arena, const uint8_t *body, size_t body_len) {
    idl_value *body_val;
    if (body && body_len > 0) {
        body_val =
            idl_value_opt_some(arena, idl_value_blob(arena, body, body_len));
    } else {
        body_val = idl_value_opt_none(arena);
    }
    return (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("body"),
                  .name = "body"},
        .value = body_val
    };
}

static idl_value_field
build_transform_field(idl_arena                    *arena,
                      const ic_transform_context_t *transform) {
    idl_value *transform_val;
    if (transform) {
        transform_val = idl_value_opt_some(
            arena, build_transform_context_value(arena, transform));
    } else {
        transform_val = idl_value_opt_none(arena);
    }
    return (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("transform"),
                  .name = "transform"},
        .value = transform_val
    };
}

static idl_value_field build_is_replicated_field(idl_arena *arena) {
    return (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("is_replicated"),
                  .name = "is_replicated"},
        .value = idl_value_opt_none(arena)
    };
}

ic_result_t build_http_request_args_candid(idl_arena                    *arena,
                                           const ic_http_request_args_t *args,
                                           idl_value **value_out) {
    idl_value_field fields[7];
    size_t          field_idx = 0;

    fields[field_idx++] = build_url_field(arena, args->url);
    fields[field_idx++] =
        build_max_response_bytes_field(arena, args->max_response_bytes);
    fields[field_idx++] = build_method_field(arena, args->method);
    fields[field_idx++] =
        build_headers_field(arena, args->headers, args->headers_count);
    fields[field_idx++] = build_body_field(arena, args->body, args->body_len);
    fields[field_idx++] = build_transform_field(arena, args->transform);
    fields[field_idx++] = build_is_replicated_field(arena);

    idl_value_fields_sort_inplace(fields, field_idx);
    *value_out = idl_value_record(arena, fields, field_idx);

    return IC_OK;
}
