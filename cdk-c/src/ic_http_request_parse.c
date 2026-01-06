// HTTP Request Response Parsing
#include "ic0.h"
#include "ic_http_request.h"
#include "idl/candid.h"
#include "idl/leb128.h"
#include <stdlib.h>
#include <string.h>

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

static ic_result_t extract_nat_value(const idl_value *value, uint64_t *out) {
    if (!value || !out) {
        return IC_ERR_INVALID_ARG;
    }

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

ic_result_t ic_http_parse_response(const uint8_t            *candid_data,
                                   size_t                    candid_len,
                                   ic_http_request_result_t *result) {
    if (!candid_data || candid_len == 0 || !result) {
        return IC_ERR_INVALID_ARG;
    }

    result->status = 0;
    result->headers = NULL;
    result->headers_count = 0;
    result->body = NULL;
    result->body_len = 0;

    idl_arena arena;
    idl_arena_init(&arena, 8192);

    idl_deserializer *deserializer = NULL;
    if (idl_deserializer_new(candid_data, candid_len, &arena, &deserializer) !=
        IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    idl_type  *type;
    idl_value *val;
    if (idl_deserializer_get_value(deserializer, &type, &val) !=
        IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    if (!val || val->kind != IDL_VALUE_RECORD) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    if (val->data.record.len == 0) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_STATE;
    }

    uint32_t status_hash = idl_hash("status");
    uint32_t headers_hash = idl_hash("headers");
    uint32_t body_hash = idl_hash("body");

    for (size_t i = 0; i < val->data.record.len; i++) {
        idl_value_field *field = &val->data.record.fields[i];
        uint32_t         field_hash = field->label.id;

        if (field_hash == status_hash) {
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

void ic_http_free_result(ic_http_request_result_t *result) {
    if (!result) {
        return;
    }

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

    if (result->body) {
        free(result->body);
        result->body = NULL;
    }

    result->status = 0;
    result->headers_count = 0;
    result->body_len = 0;
}

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

ic_result_t
ic_http_get_and_parse_response_from_callback(ic_http_request_result_t *result) {
    if (!result) {
        return IC_ERR_INVALID_ARG;
    }

    result->status = 0;
    result->headers = NULL;
    result->headers_count = 0;
    result->body = NULL;
    result->body_len = 0;

    uint32_t response_size = ic0_msg_arg_data_size();

    if (response_size == 0) {
        return IC_ERR_INVALID_STATE;
    }

    uint8_t *response_data = (uint8_t *)malloc(response_size);
    if (!response_data) {
        return IC_ERR_OUT_OF_MEMORY;
    }

    ic0_msg_arg_data_copy((uintptr_t)response_data, 0, response_size);

    ic_result_t parse_res =
        ic_http_parse_response(response_data, response_size, result);

    free(response_data);

    return parse_res;
}

ic_result_t ic_http_get_reject_info(ic_http_reject_info_t *info) {
    if (!info) {
        return IC_ERR_INVALID_ARG;
    }

    info->code = 0;
    info->message = NULL;
    info->message_len = 0;

    info->code = ic_api_msg_reject_code();

    char        temp_buf[512];
    size_t      reject_len = 0;
    ic_result_t msg_result =
        ic_api_msg_reject_message(temp_buf, sizeof(temp_buf), &reject_len);

    if (msg_result == IC_OK && reject_len > 0) {
        info->message = (char *)malloc(reject_len + 1);
        if (!info->message) {
            return IC_ERR_OUT_OF_MEMORY;
        }

        memcpy(info->message, temp_buf, reject_len);
        info->message[reject_len] = '\0';
        info->message_len = reject_len;
    }

    return IC_OK;
}
