/*
 * Candid Deserializer Implementation
 *
 * Implements low-level deserialization of Candid wire format into idl_value
 * objects. Provides:
 * - Primitive type readers (bool, integers, floats, text, blob, principal)
 * - Composite type readers (opt, vec, record, variant)
 * - LEB128 decoding for variable-length integers and lengths
 * - Byte-level input stream management with position tracking
 * - Type-driven deserialization using type definitions
 *
 * The deserializer reads from a byte buffer sequentially, parsing values
 * according to their types. Used by higher-level APIs to convert Candid
 * wire format into in-memory value representations.
 */
#include "idl/deserializer.h"

#include <string.h>

#include "idl/leb128.h"

/* Low-level read functions */

idl_status idl_deserializer_read_bytes(idl_deserializer *de,
                                       size_t            len,
                                       const uint8_t   **out) {
    if (!de || !out) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    if (de->pos + len > de->input_len) {
        return IDL_STATUS_ERR_TRUNCATED;
    }
    *out = de->input + de->pos;
    de->pos += len;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_byte(idl_deserializer *de, uint8_t *out) {
    if (!de || !out) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    if (de->pos >= de->input_len) {
        return IDL_STATUS_ERR_TRUNCATED;
    }
    *out = de->input[de->pos++];
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_leb128(idl_deserializer *de, uint64_t *out) {
    if (!de || !out) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    size_t     consumed;
    idl_status st = idl_uleb128_decode(de->input + de->pos,
                                       de->input_len - de->pos, &consumed, out);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    de->pos += consumed;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_sleb128(idl_deserializer *de, int64_t *out) {
    if (!de || !out) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    size_t     consumed;
    idl_status st = idl_sleb128_decode(de->input + de->pos,
                                       de->input_len - de->pos, &consumed, out);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    de->pos += consumed;
    return IDL_STATUS_OK;
}

/* Primitive readers */

idl_status idl_deserializer_read_bool(idl_deserializer *de, int *out) {
    uint8_t    byte;
    idl_status st = idl_deserializer_read_byte(de, &byte);
    if (st != IDL_STATUS_OK)
        return st;
    if (byte > 1) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    *out = byte;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_nat8(idl_deserializer *de, uint8_t *out) {
    return idl_deserializer_read_byte(de, out);
}

idl_status idl_deserializer_read_nat16(idl_deserializer *de, uint16_t *out) {
    const uint8_t *bytes;
    idl_status     st = idl_deserializer_read_bytes(de, 2, &bytes);
    if (st != IDL_STATUS_OK)
        return st;
    *out = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_nat32(idl_deserializer *de, uint32_t *out) {
    const uint8_t *bytes;
    idl_status     st = idl_deserializer_read_bytes(de, 4, &bytes);
    if (st != IDL_STATUS_OK)
        return st;
    *out = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_nat64(idl_deserializer *de, uint64_t *out) {
    const uint8_t *bytes;
    idl_status     st = idl_deserializer_read_bytes(de, 8, &bytes);
    if (st != IDL_STATUS_OK)
        return st;
    *out = 0;
    for (int i = 0; i < 8; i++) {
        *out |= ((uint64_t)bytes[i] << (i * 8));
    }
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_int8(idl_deserializer *de, int8_t *out) {
    uint8_t    byte;
    idl_status st = idl_deserializer_read_byte(de, &byte);
    if (st != IDL_STATUS_OK)
        return st;
    *out = (int8_t)byte;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_int16(idl_deserializer *de, int16_t *out) {
    uint16_t   val;
    idl_status st = idl_deserializer_read_nat16(de, &val);
    if (st != IDL_STATUS_OK)
        return st;
    *out = (int16_t)val;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_int32(idl_deserializer *de, int32_t *out) {
    uint32_t   val;
    idl_status st = idl_deserializer_read_nat32(de, &val);
    if (st != IDL_STATUS_OK)
        return st;
    *out = (int32_t)val;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_int64(idl_deserializer *de, int64_t *out) {
    uint64_t   val;
    idl_status st = idl_deserializer_read_nat64(de, &val);
    if (st != IDL_STATUS_OK)
        return st;
    *out = (int64_t)val;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_float32(idl_deserializer *de, float *out) {
    uint32_t   val;
    idl_status st = idl_deserializer_read_nat32(de, &val);
    if (st != IDL_STATUS_OK)
        return st;

    union {
        uint32_t u;
        float    f;
    } conv;

    conv.u = val;
    *out = conv.f;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_float64(idl_deserializer *de, double *out) {
    uint64_t   val;
    idl_status st = idl_deserializer_read_nat64(de, &val);
    if (st != IDL_STATUS_OK)
        return st;

    union {
        uint64_t u;
        double   d;
    } conv;

    conv.u = val;
    *out = conv.d;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_text(idl_deserializer *de,
                                      const char      **out,
                                      size_t           *out_len) {
    uint64_t   len;
    idl_status st = idl_deserializer_read_leb128(de, &len);
    if (st != IDL_STATUS_OK)
        return st;

    const uint8_t *bytes;
    st = idl_deserializer_read_bytes(de, (size_t)len, &bytes);
    if (st != IDL_STATUS_OK)
        return st;

    /* Copy to arena with null terminator */
    char *text = idl_arena_alloc(de->arena, (size_t)len + 1);
    if (!text)
        return IDL_STATUS_ERR_ALLOC;
    memcpy(text, bytes, (size_t)len);
    text[len] = '\0';

    *out = text;
    *out_len = (size_t)len;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_blob(idl_deserializer *de,
                                      const uint8_t   **out,
                                      size_t           *out_len) {
    uint64_t   len;
    idl_status st = idl_deserializer_read_leb128(de, &len);
    if (st != IDL_STATUS_OK)
        return st;

    const uint8_t *bytes;
    st = idl_deserializer_read_bytes(de, (size_t)len, &bytes);
    if (st != IDL_STATUS_OK)
        return st;

    /* Copy to arena */
    uint8_t *blob = idl_arena_alloc(de->arena, (size_t)len);
    if (!blob && len > 0)
        return IDL_STATUS_ERR_ALLOC;
    if (len > 0) {
        memcpy(blob, bytes, (size_t)len);
    }

    *out = blob;
    *out_len = (size_t)len;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_read_principal(idl_deserializer *de,
                                           const uint8_t   **out,
                                           size_t           *out_len) {
    /* Principal: 0x01 flag + length + bytes */
    uint8_t    flag;
    idl_status st = idl_deserializer_read_byte(de, &flag);
    if (st != IDL_STATUS_OK)
        return st;
    if (flag != 1) {
        return IDL_STATUS_ERR_INVALID_ARG; /* opaque reference not supported */
    }

    uint64_t len;
    st = idl_deserializer_read_leb128(de, &len);
    if (st != IDL_STATUS_OK)
        return st;
    if (len > 29) {
        return IDL_STATUS_ERR_INVALID_ARG; /* principal too long */
    }

    const uint8_t *bytes;
    st = idl_deserializer_read_bytes(de, (size_t)len, &bytes);
    if (st != IDL_STATUS_OK)
        return st;

    /* Copy to arena */
    uint8_t *principal = idl_arena_alloc(de->arena, (size_t)len);
    if (!principal && len > 0)
        return IDL_STATUS_ERR_ALLOC;
    if (len > 0) {
        memcpy(principal, bytes, (size_t)len);
    }

    *out = principal;
    *out_len = (size_t)len;
    return IDL_STATUS_OK;
}

/* Read value by wire type (recursive) */

static idl_status idl_deserializer_read_value_null(idl_deserializer *de,
                                                   idl_value       **out) {
    *out = idl_value_null(de->arena);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_bool(idl_deserializer *de,
                                                   idl_value       **out) {
    int        val;
    idl_status st = idl_deserializer_read_bool(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_bool(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_nat8(idl_deserializer *de,
                                                   idl_value       **out) {
    uint8_t    val;
    idl_status st = idl_deserializer_read_nat8(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_nat8(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_nat16(idl_deserializer *de,
                                                    idl_value       **out) {
    uint16_t   val;
    idl_status st = idl_deserializer_read_nat16(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_nat16(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_nat32(idl_deserializer *de,
                                                    idl_value       **out) {
    uint32_t   val;
    idl_status st = idl_deserializer_read_nat32(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_nat32(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_nat64(idl_deserializer *de,
                                                    idl_value       **out) {
    uint64_t   val;
    idl_status st = idl_deserializer_read_nat64(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_nat64(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_int8(idl_deserializer *de,
                                                   idl_value       **out) {
    int8_t     val;
    idl_status st = idl_deserializer_read_int8(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_int8(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_int16(idl_deserializer *de,
                                                    idl_value       **out) {
    int16_t    val;
    idl_status st = idl_deserializer_read_int16(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_int16(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_int32(idl_deserializer *de,
                                                    idl_value       **out) {
    int32_t    val;
    idl_status st = idl_deserializer_read_int32(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_int32(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_int64(idl_deserializer *de,
                                                    idl_value       **out) {
    int64_t    val;
    idl_status st = idl_deserializer_read_int64(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_int64(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_float32(idl_deserializer *de,
                                                      idl_value       **out) {
    float      val;
    idl_status st = idl_deserializer_read_float32(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_float32(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_float64(idl_deserializer *de,
                                                      idl_value       **out) {
    double     val;
    idl_status st = idl_deserializer_read_float64(de, &val);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_float64(de->arena, val);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_text(idl_deserializer *de,
                                                   idl_value       **out) {
    const char *text;
    size_t      len;
    idl_status  st = idl_deserializer_read_text(de, &text, &len);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_text(de->arena, text, len);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_reserved(idl_deserializer *de,
                                                       idl_value       **out) {
    *out = idl_value_reserved(de->arena);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_principal(idl_deserializer *de,
                                                        idl_value       **out) {
    const uint8_t *data;
    size_t         len;
    idl_status     st = idl_deserializer_read_principal(de, &data, &len);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *out = idl_value_principal(de->arena, data, len);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_nat(idl_deserializer *de,
                                                  idl_value       **out) {
    size_t     start = de->pos;
    uint64_t   dummy;
    idl_status st = idl_deserializer_read_leb128(de, &dummy);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    size_t len = de->pos - start;

    *out = idl_value_nat_bytes(de->arena, de->input + start, len);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_int(idl_deserializer *de,
                                                  idl_value       **out) {
    size_t     start = de->pos;
    int64_t    dummy;
    idl_status st = idl_deserializer_read_sleb128(de, &dummy);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    size_t len = de->pos - start;

    *out = idl_value_int_bytes(de->arena, de->input + start, len);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_opt(idl_deserializer *de,
                                                  const idl_type   *actual_type,
                                                  idl_value       **out) {
    uint8_t    flag;
    idl_status st = idl_deserializer_read_byte(de, &flag);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    if (flag == 0) {
        *out = idl_value_opt_none(de->arena);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    if (flag == 1) {
        idl_value *inner;
        st = idl_deserializer_read_value_by_type(de, actual_type->data.inner,
                                                 &inner);
        if (st != IDL_STATUS_OK) {
            return st;
        }
        *out = idl_value_opt_some(de->arena, inner);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    return IDL_STATUS_ERR_INVALID_ARG;
}

static idl_status idl_deserializer_read_value_vec(idl_deserializer *de,
                                                  const idl_type   *actual_type,
                                                  idl_value       **out) {
    uint64_t   len;
    idl_status st = idl_deserializer_read_leb128(de, &len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    const idl_type *inner_type = actual_type->data.inner;
    if (inner_type->kind == IDL_KIND_VAR) {
        inner_type = idl_type_env_trace(&de->header.env, inner_type);
    }
    if (inner_type && inner_type->kind == IDL_KIND_NAT8) {
        const uint8_t *bytes;
        st = idl_deserializer_read_bytes(de, (size_t)len, &bytes);
        if (st != IDL_STATUS_OK) {
            return st;
        }
        *out = idl_value_blob(de->arena, bytes, (size_t)len);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    idl_value **items =
        idl_arena_alloc(de->arena, sizeof(idl_value *) * (size_t)len);
    if (!items && len > 0) {
        return IDL_STATUS_ERR_ALLOC;
    }

    for (uint64_t i = 0; i < len; i++) {
        st = idl_deserializer_read_value_by_type(de, actual_type->data.inner,
                                                 &items[i]);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    *out = idl_value_vec(de->arena, items, (size_t)len);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_record(
    idl_deserializer *de, const idl_type *actual_type, idl_value **out) {
    size_t           field_count = actual_type->data.record.fields_len;
    idl_value_field *fields =
        idl_arena_alloc(de->arena, sizeof(idl_value_field) * field_count);
    if (!fields && field_count > 0) {
        return IDL_STATUS_ERR_ALLOC;
    }

    for (size_t i = 0; i < field_count; i++) {
        idl_field *type_field = &actual_type->data.record.fields[i];
        fields[i].label = type_field->label;

        idl_status st = idl_deserializer_read_value_by_type(
            de, type_field->type, &fields[i].value);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    *out = idl_value_record(de->arena, fields, field_count);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

static idl_status idl_deserializer_read_value_variant(
    idl_deserializer *de, const idl_type *actual_type, idl_value **out) {
    uint64_t   index;
    idl_status st = idl_deserializer_read_leb128(de, &index);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    if (index >= actual_type->data.record.fields_len) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    idl_field      *type_field = &actual_type->data.record.fields[index];
    idl_value_field field;
    field.label = type_field->label;

    st =
        idl_deserializer_read_value_by_type(de, type_field->type, &field.value);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    *out = idl_value_variant(de->arena, index, &field);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

idl_status idl_deserializer_read_value_by_type(idl_deserializer *de,
                                               const idl_type   *wire_type,
                                               idl_value       **out) {
    if (!de || !wire_type || !out) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    /* Resolve VAR types */
    const idl_type *actual_type = wire_type;
    if (wire_type->kind == IDL_KIND_VAR) {
        actual_type = idl_type_env_trace(&de->header.env, wire_type);
        if (!actual_type) {
            return IDL_STATUS_ERR_INVALID_ARG;
        }
    }

    idl_status st;

    switch (actual_type->kind) {
    case IDL_KIND_NULL:
        return idl_deserializer_read_value_null(de, out);

    case IDL_KIND_BOOL:
        return idl_deserializer_read_value_bool(de, out);

    case IDL_KIND_NAT8:
        return idl_deserializer_read_value_nat8(de, out);

    case IDL_KIND_NAT16:
        return idl_deserializer_read_value_nat16(de, out);

    case IDL_KIND_NAT32:
        return idl_deserializer_read_value_nat32(de, out);

    case IDL_KIND_NAT64:
        return idl_deserializer_read_value_nat64(de, out);

    case IDL_KIND_INT8:
        return idl_deserializer_read_value_int8(de, out);

    case IDL_KIND_INT16:
        return idl_deserializer_read_value_int16(de, out);

    case IDL_KIND_INT32:
        return idl_deserializer_read_value_int32(de, out);

    case IDL_KIND_INT64:
        return idl_deserializer_read_value_int64(de, out);

    case IDL_KIND_FLOAT32:
        return idl_deserializer_read_value_float32(de, out);

    case IDL_KIND_FLOAT64:
        return idl_deserializer_read_value_float64(de, out);

    case IDL_KIND_TEXT:
        return idl_deserializer_read_value_text(de, out);

    case IDL_KIND_RESERVED:
        return idl_deserializer_read_value_reserved(de, out);

    case IDL_KIND_EMPTY:
        return IDL_STATUS_ERR_INVALID_ARG; /* cannot decode empty type */

    case IDL_KIND_PRINCIPAL:
        return idl_deserializer_read_value_principal(de, out);

    case IDL_KIND_NAT:
        return idl_deserializer_read_value_nat(de, out);

    case IDL_KIND_INT:
        return idl_deserializer_read_value_int(de, out);

    case IDL_KIND_OPT:
        return idl_deserializer_read_value_opt(de, actual_type, out);

    case IDL_KIND_VEC:
        return idl_deserializer_read_value_vec(de, actual_type, out);

    case IDL_KIND_RECORD:
        return idl_deserializer_read_value_record(de, actual_type, out);

    case IDL_KIND_VARIANT:
        return idl_deserializer_read_value_variant(de, actual_type, out);

    case IDL_KIND_FUNC:
    case IDL_KIND_SERVICE:
        /* TODO: implement func/service decoding */
        return IDL_STATUS_ERR_UNSUPPORTED;

    case IDL_KIND_VAR:
        /* Should have been resolved above */
        return IDL_STATUS_ERR_INVALID_ARG;

    default:
        return IDL_STATUS_ERR_UNSUPPORTED;
    }
}

idl_status idl_deserializer_get_value(idl_deserializer *de,
                                      idl_type        **out_type,
                                      idl_value       **out_value) {
    if (!de || !out_type || !out_value) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    if (de->arg_index >= de->header.arg_count) {
        return IDL_STATUS_ERR_INVALID_ARG; /* no more values */
    }

    idl_type *wire_type = de->header.arg_types[de->arg_index];
    de->arg_index++;

    idl_status st =
        idl_deserializer_read_value_by_type(de, wire_type, out_value);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    *out_type = wire_type;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_get_value_with_type(idl_deserializer *de,
                                                const idl_type   *expected_type,
                                                idl_value       **out_value) {
    if (!de || !expected_type || !out_value) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    /* Get the wire type and value */
    idl_type  *wire_type;
    idl_value *wire_value;
    idl_status st = idl_deserializer_get_value(de, &wire_type, &wire_value);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    /* Coerce to expected type */
    /* Note: coerce.h is included via runtime.h */
    extern idl_status idl_coerce_value(
        idl_arena * arena, const idl_type_env *env, const idl_type *wire_type,
        const idl_type *expected_type, const idl_value *value, idl_value **out);

    return idl_coerce_value(de->arena, &de->header.env, wire_type,
                            expected_type, wire_value, out_value);
}

idl_status idl_deserializer_add_cost(idl_deserializer *de, size_t cost) {
    if (!de) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    de->cost_accumulated += cost;

    if (de->config.decoding_quota > 0) {
        if (de->cost_accumulated > de->config.decoding_quota) {
            return IDL_STATUS_ERR_OVERFLOW; /* quota exceeded */
        }
    }

    return IDL_STATUS_OK;
}

size_t idl_deserializer_get_cost(const idl_deserializer *de) {
    return de ? de->cost_accumulated : 0;
}

const idl_decoder_config *
idl_deserializer_get_config(const idl_deserializer *de) {
    return de ? &de->config : NULL;
}
