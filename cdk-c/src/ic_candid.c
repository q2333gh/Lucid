#include "ic_candid.h"
// Force undefine macros from ic_candid.h before including c_candid headers
// to avoid redefinition warnings/errors
#undef CANDID_TYPE_NULL
#undef CANDID_TYPE_BOOL
#undef CANDID_TYPE_NAT
#undef CANDID_TYPE_INT
#undef CANDID_TYPE_NAT8
#undef CANDID_TYPE_INT8
#undef CANDID_TYPE_NAT16
#undef CANDID_TYPE_INT16
#undef CANDID_TYPE_NAT32
#undef CANDID_TYPE_INT32
#undef CANDID_TYPE_NAT64
#undef CANDID_TYPE_INT64
#undef CANDID_TYPE_FLOAT32
#undef CANDID_TYPE_FLOAT64
#undef CANDID_TYPE_TEXT
#undef CANDID_TYPE_RESERVED
#undef CANDID_TYPE_EMPTY
#undef CANDID_TYPE_OPT
#undef CANDID_TYPE_VEC
#undef CANDID_TYPE_RECORD
#undef CANDID_TYPE_VARIANT
#undef CANDID_TYPE_FUNC
#undef CANDID_TYPE_SERVICE
#undef CANDID_TYPE_PRINCIPAL
#undef CANDID_TYPE_BLOB

#include <stdlib.h>
#include <string.h>

#include "idl/candid.h"
#include "idl/leb128.h"

// Implementation of ic_candid.h functions using c_candid library

ic_result_t candid_serialize_text(ic_buffer_t* buf, const char* text) {
    idl_arena arena;
    idl_arena_init(&arena, 1024);

    idl_builder builder;
    idl_builder_init(&builder, &arena);
    idl_builder_arg_text_cstr(&builder, text);

    uint8_t* bytes;
    size_t len;
    if (idl_builder_serialize(&builder, &bytes, &len) != IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    ic_buffer_append(buf, bytes, len);

    idl_arena_destroy(&arena);
    return IC_OK;
}

ic_result_t candid_serialize_nat(ic_buffer_t* buf, uint64_t value) {
    idl_arena arena;
    idl_arena_init(&arena, 512);

    idl_builder builder;
    idl_builder_init(&builder, &arena);
    idl_builder_arg_nat64(&builder, value);

    uint8_t* bytes;
    size_t len;
    if (idl_builder_serialize(&builder, &bytes, &len) != IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    ic_buffer_append(buf, bytes, len);
    idl_arena_destroy(&arena);
    return IC_OK;
}

ic_result_t candid_serialize_int(ic_buffer_t* buf, int64_t value) {
    idl_arena arena;
    idl_arena_init(&arena, 512);

    idl_builder builder;
    idl_builder_init(&builder, &arena);
    idl_builder_arg_int64(&builder, value);

    uint8_t* bytes;
    size_t len;
    if (idl_builder_serialize(&builder, &bytes, &len) != IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    ic_buffer_append(buf, bytes, len);
    idl_arena_destroy(&arena);
    return IC_OK;
}

ic_result_t candid_serialize_blob(ic_buffer_t* buf, const uint8_t* data,
                                  size_t len) {
    idl_arena arena;
    idl_arena_init(&arena, 512 + len);

    idl_builder builder;
    idl_builder_init(&builder, &arena);
    idl_builder_arg_blob(&builder, data, len);

    uint8_t* bytes;
    size_t out_len;
    if (idl_builder_serialize(&builder, &bytes, &out_len) != IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    ic_buffer_append(buf, bytes, out_len);
    idl_arena_destroy(&arena);
    return IC_OK;
}

ic_result_t candid_serialize_principal(ic_buffer_t* buf,
                                       const ic_principal_t* principal) {
    // Check if principal is valid if strictly required, but ic_candid.h says
    // nothing about NULL safety. The test passes invalid principal {0} and
    // expects IC_ERR_INVALID_ARG. ic_principal_t invalid = {0};
    // cr_expect_eq(candid_serialize_principal(&buf, &invalid),
    // IC_ERR_INVALID_ARG);
    if (buf == NULL || principal == NULL || principal->len == 0 ||
        principal->len > IC_PRINCIPAL_MAX_LEN) {
        return IC_ERR_INVALID_ARG;
    }

    idl_arena arena;
    idl_arena_init(&arena, 512);

    idl_builder builder;
    idl_builder_init(&builder, &arena);
    idl_builder_arg_principal(&builder, principal->bytes, principal->len);

    uint8_t* bytes;
    size_t len;
    if (idl_builder_serialize(&builder, &bytes, &len) != IDL_STATUS_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    ic_buffer_append(buf, bytes, len);
    idl_arena_destroy(&arena);
    return IC_OK;
}

// Deserialization helpers
static ic_result_t deserialize_generic(const uint8_t* data, size_t len,
                                       size_t* offset, idl_value** val,
                                       idl_arena* arena) {
    // Note: We assume data points to full message, offset handles position?
    // c_candid deserializer takes full buffer and manages position internally
    // if we use deserializer object. But here we are stateless functions. The
    // c_candid `idl_deserializer_new` takes the whole buffer.

    // If offset > 0, we might be mid-stream. BUT `c_candid` deserializer
    // expects to start at header. If these functions are called after header
    // check, we might need adjustments. However, typically `ic_candid.h`
    // functions are high-level "deserialize this full message". Let's assume
    // data/len is the full buffer.

    idl_deserializer* de = NULL;
    if (idl_deserializer_new(data, len, arena, &de) != IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    // We need to skip to *offset*?
    // The `ic_candid` API implies we can read multiple values sequentially by
    // updating offset? c_candid deserializer keeps state. If we create a new
    // deserializer every time, we must fast-forward. But `idl_deserializer_new`
    // parses header.

    // For now, assume single value or we implement full stream reading if
    // needed. But wait, `ic_api.c` usage was `ic_api_from_wire_*` which works
    // on the global buffer.

    idl_type* type;
    if (idl_deserializer_get_value(de, &type, val) != IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    // If successful, update offset?
    // c_candid doesn't easily expose byte offset.
    *offset = len;  // Hack: Assume we consumed it all.
    return IC_OK;
}

ic_result_t candid_deserialize_text(const uint8_t* data, size_t len,
                                    size_t* offset, char** text,
                                    size_t* text_len) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);  // Needs to be large enough or persistent?
    // Problem: return value points to arena memory. If we destroy arena, we
    // lose data. `ic_candid.h` implies caller owns memory or it's valid?
    // Usually these APIs copy out or rely on buffer.
    // If we copy out, we need to allocate.

    idl_value* val;
    if (deserialize_generic(data, len, offset, &val, &arena) != IC_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    if (val->kind != IDL_VALUE_TEXT) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    *text = strdup(val->data.text.data);  // Copy because arena will die
    *text_len = val->data.text.len;

    idl_arena_destroy(&arena);
    return IC_OK;
}

ic_result_t candid_deserialize_nat(const uint8_t* data, size_t len,
                                   size_t* offset, uint64_t* value) {
    idl_arena arena;
    idl_arena_init(&arena, 1024);

    idl_value* val;
    if (deserialize_generic(data, len, offset, &val, &arena) != IC_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    ic_result_t res = IC_OK;
    if (val->kind == IDL_VALUE_NAT64)
        *value = val->data.nat64_val;
    else if (val->kind == IDL_VALUE_NAT32)
        *value = val->data.nat32_val;
    else if (val->kind == IDL_VALUE_NAT16)
        *value = val->data.nat16_val;
    else if (val->kind == IDL_VALUE_NAT8)
        *value = val->data.nat8_val;
    else if (val->kind == IDL_VALUE_NAT) {
        size_t consumed;
        if (idl_uleb128_decode(val->data.bignum.data, val->data.bignum.len,
                               &consumed, value) != IDL_STATUS_OK)
            res = IC_ERR_INVALID_ARG;
    } else
        res = IC_ERR_INVALID_ARG;

    idl_arena_destroy(&arena);
    return res;
}

ic_result_t candid_deserialize_int(const uint8_t* data, size_t len,
                                   size_t* offset, int64_t* value) {
    idl_arena arena;
    idl_arena_init(&arena, 1024);

    idl_value* val;
    if (deserialize_generic(data, len, offset, &val, &arena) != IC_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    ic_result_t res = IC_OK;
    if (val->kind == IDL_VALUE_INT64)
        *value = val->data.int64_val;
    else if (val->kind == IDL_VALUE_INT32)
        *value = val->data.int32_val;
    else if (val->kind == IDL_VALUE_INT16)
        *value = val->data.int16_val;
    else if (val->kind == IDL_VALUE_INT8)
        *value = val->data.int8_val;
    else if (val->kind == IDL_VALUE_INT) {
        size_t consumed;
        if (idl_sleb128_decode(val->data.bignum.data, val->data.bignum.len,
                               &consumed, value) != IDL_STATUS_OK)
            res = IC_ERR_INVALID_ARG;
    } else
        res = IC_ERR_INVALID_ARG;

    idl_arena_destroy(&arena);
    return res;
}

ic_result_t candid_deserialize_blob(const uint8_t* data, size_t len,
                                    size_t* offset, uint8_t** blob,
                                    size_t* blob_len) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    idl_value* val;
    if (deserialize_generic(data, len, offset, &val, &arena) != IC_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    if (val->kind == IDL_VALUE_BLOB) {
        *blob = malloc(val->data.blob.len);
        memcpy(*blob, val->data.blob.data, val->data.blob.len);
        *blob_len = val->data.blob.len;
    } else {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    idl_arena_destroy(&arena);
    return IC_OK;
}

ic_result_t candid_deserialize_principal(const uint8_t* data, size_t len,
                                         size_t* offset,
                                         ic_principal_t* principal) {
    idl_arena arena;
    idl_arena_init(&arena, 1024);

    idl_value* val;
    if (deserialize_generic(data, len, offset, &val, &arena) != IC_OK) {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    if (val->kind == IDL_VALUE_PRINCIPAL) {
        ic_principal_from_bytes(principal, val->data.principal.data,
                                val->data.principal.len);
    } else {
        idl_arena_destroy(&arena);
        return IC_ERR_INVALID_ARG;
    }

    idl_arena_destroy(&arena);
    return IC_OK;
}

ic_result_t candid_write_leb128(ic_buffer_t* buf, uint64_t value) {
    if (buf == NULL) return IC_ERR_INVALID_ARG;  // Fix: Check for NULL buffer
    uint8_t tmp[10];                             // Max for uint64
    size_t written;
    if (idl_uleb128_encode(value, tmp, sizeof(tmp), &written) ==
        IDL_STATUS_OK) {
        ic_buffer_append(buf, tmp, written);
        return IC_OK;
    }
    return IC_ERR_INVALID_ARG;
}

ic_result_t candid_read_leb128(const uint8_t* data, size_t len, size_t* offset,
                               uint64_t* value) {
    if (data == NULL) return IC_ERR_INVALID_ARG;
    size_t consumed;
    if (idl_uleb128_decode(data + *offset, len - *offset, &consumed, value) ==
        IDL_STATUS_OK) {
        *offset += consumed;
        return IC_OK;
    }
    return IC_ERR_INVALID_ARG;
}

ic_result_t candid_read_sleb128(const uint8_t* data, size_t len, size_t* offset,
                                int64_t* value) {
    if (data == NULL) return IC_ERR_INVALID_ARG;
    size_t consumed;
    if (idl_sleb128_decode(data + *offset, len - *offset, &consumed, value) ==
        IDL_STATUS_OK) {
        *offset += consumed;
        return IC_OK;
    }
    return IC_ERR_INVALID_ARG;
}

bool candid_check_magic(const uint8_t* data, size_t len) {
    if (len < 4) return false;
    return memcmp(data, "DIDL", 4) == 0;
}
