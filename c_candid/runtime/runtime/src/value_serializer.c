/*
 * Candid Value Serializer
 *
 * Serializes idl_value objects to Candid wire format bytes. Handles:
 * - Primitive value encoding (integers, floats, text, blobs, principals)
 * - Composite value encoding (opt, vec, record, variant)
 * - LEB128 encoding for lengths and variable-width integers
 * - Dynamic buffer growth for variable-length data
 *
 * Works in conjunction with the type table builder to produce complete
 * Candid messages. The serializer maintains its own buffer and appends
 * encoded values sequentially.
 */
#include "idl/value_serializer.h"

#include <string.h>

#include "idl/leb128.h"

#define INITIAL_CAPACITY 256
#define MAX_LEB128_SIZE 10

/* Ensure capacity for additional bytes */
static idl_status ensure_capacity(idl_value_serializer *ser,
                                  size_t                additional) {
    size_t needed = ser->len + additional;
    if (needed <= ser->capacity) {
        return IDL_STATUS_OK;
    }

    size_t new_cap = ser->capacity == 0 ? INITIAL_CAPACITY : ser->capacity;
    while (new_cap < needed) {
        new_cap *= 2;
    }

    uint8_t *new_data = idl_arena_alloc(ser->arena, new_cap);
    if (!new_data) {
        return IDL_STATUS_ERR_ALLOC;
    }

    if (ser->data && ser->len > 0) {
        memcpy(new_data, ser->data, ser->len);
    }

    ser->data = new_data;
    ser->capacity = new_cap;
    return IDL_STATUS_OK;
}

idl_status idl_value_serializer_init(idl_value_serializer *ser,
                                     idl_arena            *arena) {
    if (!ser || !arena) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    ser->arena = arena;
    ser->data = NULL;
    ser->len = 0;
    ser->capacity = 0;

    return IDL_STATUS_OK;
}

const uint8_t *idl_value_serializer_data(const idl_value_serializer *ser) {
    return ser ? ser->data : NULL;
}

size_t idl_value_serializer_len(const idl_value_serializer *ser) {
    return ser ? ser->len : 0;
}

idl_status idl_value_serializer_write(idl_value_serializer *ser,
                                      const uint8_t        *data,
                                      size_t                len) {
    if (!ser) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return IDL_STATUS_OK;
    }

    idl_status st = ensure_capacity(ser, len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    memcpy(ser->data + ser->len, data, len);
    ser->len += len;
    return IDL_STATUS_OK;
}

idl_status idl_value_serializer_write_byte(idl_value_serializer *ser,
                                           uint8_t               byte) {
    return idl_value_serializer_write(ser, &byte, 1);
}

idl_status idl_value_serializer_write_leb128(idl_value_serializer *ser,
                                             uint64_t              value) {
    uint8_t    buf[MAX_LEB128_SIZE];
    size_t     written;
    idl_status st = idl_uleb128_encode(value, buf, sizeof(buf), &written);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    return idl_value_serializer_write(ser, buf, written);
}

idl_status idl_value_serializer_write_sleb128(idl_value_serializer *ser,
                                              int64_t               value) {
    uint8_t    buf[MAX_LEB128_SIZE];
    size_t     written;
    idl_status st = idl_sleb128_encode(value, buf, sizeof(buf), &written);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    return idl_value_serializer_write(ser, buf, written);
}

/* Primitive serialization */

idl_status idl_value_serializer_write_null(idl_value_serializer *ser) {
    /* Null has no wire representation */
    (void)ser;
    return IDL_STATUS_OK;
}

idl_status idl_value_serializer_write_bool(idl_value_serializer *ser, int v) {
    uint8_t byte = v ? 1 : 0;
    return idl_value_serializer_write_byte(ser, byte);
}

idl_status idl_value_serializer_write_nat8(idl_value_serializer *ser,
                                           uint8_t               v) {
    return idl_value_serializer_write_byte(ser, v);
}

idl_status idl_value_serializer_write_nat16(idl_value_serializer *ser,
                                            uint16_t              v) {
    /* Little-endian */
    uint8_t buf[2];
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
    return idl_value_serializer_write(ser, buf, 2);
}

idl_status idl_value_serializer_write_nat32(idl_value_serializer *ser,
                                            uint32_t              v) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
    buf[2] = (uint8_t)((v >> 16) & 0xFF);
    buf[3] = (uint8_t)((v >> 24) & 0xFF);
    return idl_value_serializer_write(ser, buf, 4);
}

idl_status idl_value_serializer_write_nat64(idl_value_serializer *ser,
                                            uint64_t              v) {
    uint8_t buf[8];
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)((v >> (i * 8)) & 0xFF);
    }
    return idl_value_serializer_write(ser, buf, 8);
}

idl_status idl_value_serializer_write_int8(idl_value_serializer *ser,
                                           int8_t                v) {
    return idl_value_serializer_write_byte(ser, (uint8_t)v);
}

idl_status idl_value_serializer_write_int16(idl_value_serializer *ser,
                                            int16_t               v) {
    return idl_value_serializer_write_nat16(ser, (uint16_t)v);
}

idl_status idl_value_serializer_write_int32(idl_value_serializer *ser,
                                            int32_t               v) {
    return idl_value_serializer_write_nat32(ser, (uint32_t)v);
}

idl_status idl_value_serializer_write_int64(idl_value_serializer *ser,
                                            int64_t               v) {
    return idl_value_serializer_write_nat64(ser, (uint64_t)v);
}

idl_status idl_value_serializer_write_float32(idl_value_serializer *ser,
                                              float                 v) {
    union {
        float    f;
        uint32_t u;
    } conv;

    conv.f = v;
    return idl_value_serializer_write_nat32(ser, conv.u);
}

idl_status idl_value_serializer_write_float64(idl_value_serializer *ser,
                                              double                v) {
    union {
        double   d;
        uint64_t u;
    } conv;

    conv.d = v;
    return idl_value_serializer_write_nat64(ser, conv.u);
}

idl_status idl_value_serializer_write_text(idl_value_serializer *ser,
                                           const char           *s,
                                           size_t                len) {
    idl_status st = idl_value_serializer_write_leb128(ser, len);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    return idl_value_serializer_write(ser, (const uint8_t *)s, len);
}

idl_status idl_value_serializer_write_blob(idl_value_serializer *ser,
                                           const uint8_t        *data,
                                           size_t                len) {
    idl_status st = idl_value_serializer_write_leb128(ser, len);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    return idl_value_serializer_write(ser, data, len);
}

idl_status idl_value_serializer_write_principal(idl_value_serializer *ser,
                                                const uint8_t        *data,
                                                size_t                len) {
    /* Principal: 0x01 flag + length + bytes */
    idl_status st = idl_value_serializer_write_byte(ser, 1);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    st = idl_value_serializer_write_leb128(ser, len);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    return idl_value_serializer_write(ser, data, len);
}

idl_status idl_value_serializer_write_reserved(idl_value_serializer *ser) {
    /* Reserved has no wire representation */
    (void)ser;
    return IDL_STATUS_OK;
}

/* Composite serialization helpers */

idl_status idl_value_serializer_write_opt_none(idl_value_serializer *ser) {
    return idl_value_serializer_write_leb128(ser, 0);
}

idl_status idl_value_serializer_write_opt_some(idl_value_serializer *ser) {
    return idl_value_serializer_write_leb128(ser, 1);
}

idl_status idl_value_serializer_write_vec_len(idl_value_serializer *ser,
                                              size_t                len) {
    return idl_value_serializer_write_leb128(ser, len);
}

idl_status idl_value_serializer_write_variant_index(idl_value_serializer *ser,
                                                    uint64_t index) {
    return idl_value_serializer_write_leb128(ser, index);
}

/* Nat/Int arbitrary precision */

idl_status idl_value_serializer_write_nat(idl_value_serializer *ser,
                                          const uint8_t        *leb_data,
                                          size_t                len) {
    return idl_value_serializer_write(ser, leb_data, len);
}

idl_status idl_value_serializer_write_int(idl_value_serializer *ser,
                                          const uint8_t        *sleb_data,
                                          size_t                len) {
    return idl_value_serializer_write(ser, sleb_data, len);
}

/* Serialize OPT value */
static idl_status write_opt_value(idl_value_serializer *ser,
                                  const idl_value      *value) {
    if (value->data.opt == NULL) {
        return idl_value_serializer_write_opt_none(ser);
    }

    idl_status st = idl_value_serializer_write_opt_some(ser);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    return idl_value_serializer_write_value(ser, value->data.opt);
}

/* Serialize VEC value */
static idl_status write_vec_value(idl_value_serializer *ser,
                                  const idl_value      *value) {
    idl_status st =
        idl_value_serializer_write_vec_len(ser, value->data.vec.len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    for (size_t i = 0; i < value->data.vec.len; i++) {
        st = idl_value_serializer_write_value(ser, value->data.vec.items[i]);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }
    return IDL_STATUS_OK;
}

/* Serialize RECORD value */
static idl_status write_record_value(idl_value_serializer *ser,
                                     const idl_value      *value) {
    for (size_t i = 0; i < value->data.record.len; i++) {
        idl_status st = idl_value_serializer_write_value(
            ser, value->data.record.fields[i].value);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }
    return IDL_STATUS_OK;
}

/* Serialize VARIANT value */
static idl_status write_variant_value(idl_value_serializer *ser,
                                      const idl_value      *value) {
    idl_status st = idl_value_serializer_write_variant_index(
        ser, value->data.record.variant_index);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    if (value->data.record.len > 0) {
        return idl_value_serializer_write_value(
            ser, value->data.record.fields[0].value);
    }
    return IDL_STATUS_OK;
}

/* Recursive value serialization */

idl_status idl_value_serializer_write_value(idl_value_serializer *ser,
                                            const idl_value      *value) {
    if (!ser || !value) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    switch (value->kind) {
    case IDL_VALUE_NULL:
        return idl_value_serializer_write_null(ser);

    case IDL_VALUE_BOOL:
        return idl_value_serializer_write_bool(ser, value->data.bool_val);

    case IDL_VALUE_NAT8:
        return idl_value_serializer_write_nat8(ser, value->data.nat8_val);

    case IDL_VALUE_NAT16:
        return idl_value_serializer_write_nat16(ser, value->data.nat16_val);

    case IDL_VALUE_NAT32:
        return idl_value_serializer_write_nat32(ser, value->data.nat32_val);

    case IDL_VALUE_NAT64:
        return idl_value_serializer_write_nat64(ser, value->data.nat64_val);

    case IDL_VALUE_INT8:
        return idl_value_serializer_write_int8(ser, value->data.int8_val);

    case IDL_VALUE_INT16:
        return idl_value_serializer_write_int16(ser, value->data.int16_val);

    case IDL_VALUE_INT32:
        return idl_value_serializer_write_int32(ser, value->data.int32_val);

    case IDL_VALUE_INT64:
        return idl_value_serializer_write_int64(ser, value->data.int64_val);

    case IDL_VALUE_FLOAT32:
        return idl_value_serializer_write_float32(ser, value->data.float32_val);

    case IDL_VALUE_FLOAT64:
        return idl_value_serializer_write_float64(ser, value->data.float64_val);

    case IDL_VALUE_TEXT:
        return idl_value_serializer_write_text(ser, value->data.text.data,
                                               value->data.text.len);

    case IDL_VALUE_BLOB:
        return idl_value_serializer_write_blob(ser, value->data.blob.data,
                                               value->data.blob.len);

    case IDL_VALUE_RESERVED:
        return idl_value_serializer_write_reserved(ser);

    case IDL_VALUE_PRINCIPAL:
        return idl_value_serializer_write_principal(
            ser, value->data.principal.data, value->data.principal.len);

    case IDL_VALUE_NAT:
        return idl_value_serializer_write_nat(ser, value->data.bignum.data,
                                              value->data.bignum.len);

    case IDL_VALUE_INT:
        return idl_value_serializer_write_int(ser, value->data.bignum.data,
                                              value->data.bignum.len);

    case IDL_VALUE_OPT:
        return write_opt_value(ser, value);

    case IDL_VALUE_VEC:
        return write_vec_value(ser, value);

    case IDL_VALUE_RECORD:
        return write_record_value(ser, value);

    case IDL_VALUE_VARIANT:
        return write_variant_value(ser, value);

    default:
        return IDL_STATUS_ERR_UNSUPPORTED;
    }
}
