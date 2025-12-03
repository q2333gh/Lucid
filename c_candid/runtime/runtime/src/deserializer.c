#include "idl/deserializer.h"

#include <string.h>

#include "idl/leb128.h"

void idl_decoder_config_init(idl_decoder_config *config) {
    if (!config)
        return;
    config->decoding_quota = 0; /* no limit */
    config->skipping_quota = 0;
    config->full_error_message = 1;
}

idl_decoder_config *
idl_decoder_config_set_decoding_quota(idl_decoder_config *config,
                                      size_t              quota) {
    if (config) {
        config->decoding_quota = quota;
    }
    return config;
}

idl_decoder_config *
idl_decoder_config_set_skipping_quota(idl_decoder_config *config,
                                      size_t              quota) {
    if (config) {
        config->skipping_quota = quota;
    }
    return config;
}

idl_status idl_deserializer_new_with_config(const uint8_t            *data,
                                            size_t                    len,
                                            idl_arena                *arena,
                                            const idl_decoder_config *config,
                                            idl_deserializer        **out) {
    if (!data || !arena || !out) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    idl_deserializer *de =
        idl_arena_alloc_zeroed(arena, sizeof(idl_deserializer));
    if (!de) {
        return IDL_STATUS_ERR_ALLOC;
    }

    de->arena = arena;
    de->input = data;
    de->input_len = len;
    de->pos = 0;
    de->arg_index = 0;

    if (config) {
        de->config = *config;
    } else {
        idl_decoder_config_init(&de->config);
    }

    /* Parse header */
    size_t     consumed;
    idl_status st = idl_header_parse(data, len, arena, &de->header, &consumed);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    de->pos = consumed;
    de->cost_accumulated = 0;

    /* Add header parsing cost */
    idl_status cost_st = idl_deserializer_add_cost(de, consumed * 4);
    if (cost_st != IDL_STATUS_OK) {
        return cost_st;
    }

    *out = de;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_new(const uint8_t     *data,
                                size_t             len,
                                idl_arena         *arena,
                                idl_deserializer **out) {
    return idl_deserializer_new_with_config(data, len, arena, NULL, out);
}

int idl_deserializer_is_done(const idl_deserializer *de) {
    return de ? (de->arg_index >= de->header.arg_count) : 1;
}

idl_status idl_deserializer_done(idl_deserializer *de) {
    if (!de) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    /* Skip any remaining arguments */
    while (!idl_deserializer_is_done(de)) {
        idl_type  *type;
        idl_value *value;
        idl_status st = idl_deserializer_get_value(de, &type, &value);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    /* Check for trailing bytes */
    if (de->pos < de->input_len) {
        return IDL_STATUS_ERR_INVALID_ARG; /* trailing bytes */
    }

    return IDL_STATUS_OK;
}

size_t idl_deserializer_remaining(const idl_deserializer *de) {
    return de ? (de->input_len - de->pos) : 0;
}

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
        *out = idl_value_null(de->arena);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;

    case IDL_KIND_BOOL: {
        int val;
        st = idl_deserializer_read_bool(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_bool(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_NAT8: {
        uint8_t val;
        st = idl_deserializer_read_nat8(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_nat8(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_NAT16: {
        uint16_t val;
        st = idl_deserializer_read_nat16(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_nat16(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_NAT32: {
        uint32_t val;
        st = idl_deserializer_read_nat32(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_nat32(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_NAT64: {
        uint64_t val;
        st = idl_deserializer_read_nat64(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_nat64(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_INT8: {
        int8_t val;
        st = idl_deserializer_read_int8(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_int8(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_INT16: {
        int16_t val;
        st = idl_deserializer_read_int16(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_int16(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_INT32: {
        int32_t val;
        st = idl_deserializer_read_int32(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_int32(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_INT64: {
        int64_t val;
        st = idl_deserializer_read_int64(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_int64(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_FLOAT32: {
        float val;
        st = idl_deserializer_read_float32(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_float32(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_FLOAT64: {
        double val;
        st = idl_deserializer_read_float64(de, &val);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_float64(de->arena, val);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_TEXT: {
        const char *text;
        size_t      len;
        st = idl_deserializer_read_text(de, &text, &len);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_text(de->arena, text, len);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_RESERVED:
        *out = idl_value_reserved(de->arena);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;

    case IDL_KIND_EMPTY:
        return IDL_STATUS_ERR_INVALID_ARG; /* cannot decode empty type */

    case IDL_KIND_PRINCIPAL: {
        const uint8_t *data;
        size_t         len;
        st = idl_deserializer_read_principal(de, &data, &len);
        if (st != IDL_STATUS_OK)
            return st;
        *out = idl_value_principal(de->arena, data, len);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_NAT: {
        /* Read ULEB128 bytes for arbitrary precision nat */
        size_t   start = de->pos;
        uint64_t dummy;
        st = idl_deserializer_read_leb128(de, &dummy);
        if (st != IDL_STATUS_OK)
            return st;
        size_t len = de->pos - start;
        /* Store the raw LEB128 bytes */
        *out = idl_value_nat_bytes(de->arena, de->input + start, len);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_INT: {
        /* Read SLEB128 bytes for arbitrary precision int */
        size_t  start = de->pos;
        int64_t dummy;
        st = idl_deserializer_read_sleb128(de, &dummy);
        if (st != IDL_STATUS_OK)
            return st;
        size_t len = de->pos - start;
        /* Store the raw SLEB128 bytes */
        *out = idl_value_int_bytes(de->arena, de->input + start, len);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_OPT: {
        uint8_t flag;
        st = idl_deserializer_read_byte(de, &flag);
        if (st != IDL_STATUS_OK)
            return st;

        if (flag == 0) {
            *out = idl_value_opt_none(de->arena);
        } else if (flag == 1) {
            idl_value *inner;
            st = idl_deserializer_read_value_by_type(
                de, actual_type->data.inner, &inner);
            if (st != IDL_STATUS_OK)
                return st;
            *out = idl_value_opt_some(de->arena, inner);
        } else {
            return IDL_STATUS_ERR_INVALID_ARG;
        }
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_VEC: {
        uint64_t len;
        st = idl_deserializer_read_leb128(de, &len);
        if (st != IDL_STATUS_OK)
            return st;

        /* Check if this is blob (vec nat8) */
        const idl_type *inner_type = actual_type->data.inner;
        if (inner_type->kind == IDL_KIND_VAR) {
            inner_type = idl_type_env_trace(&de->header.env, inner_type);
        }
        if (inner_type && inner_type->kind == IDL_KIND_NAT8) {
            /* Special case: blob */
            const uint8_t *bytes;
            st = idl_deserializer_read_bytes(de, (size_t)len, &bytes);
            if (st != IDL_STATUS_OK)
                return st;
            *out = idl_value_blob(de->arena, bytes, (size_t)len);
            return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
        }

        /* General vec */
        idl_value **items =
            idl_arena_alloc(de->arena, sizeof(idl_value *) * (size_t)len);
        if (!items && len > 0)
            return IDL_STATUS_ERR_ALLOC;

        for (uint64_t i = 0; i < len; i++) {
            st = idl_deserializer_read_value_by_type(
                de, actual_type->data.inner, &items[i]);
            if (st != IDL_STATUS_OK)
                return st;
        }

        *out = idl_value_vec(de->arena, items, (size_t)len);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_RECORD: {
        size_t           field_count = actual_type->data.record.fields_len;
        idl_value_field *fields =
            idl_arena_alloc(de->arena, sizeof(idl_value_field) * field_count);
        if (!fields && field_count > 0)
            return IDL_STATUS_ERR_ALLOC;

        for (size_t i = 0; i < field_count; i++) {
            idl_field *type_field = &actual_type->data.record.fields[i];
            fields[i].label = type_field->label;

            st = idl_deserializer_read_value_by_type(de, type_field->type,
                                                     &fields[i].value);
            if (st != IDL_STATUS_OK)
                return st;
        }

        *out = idl_value_record(de->arena, fields, field_count);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    case IDL_KIND_VARIANT: {
        uint64_t index;
        st = idl_deserializer_read_leb128(de, &index);
        if (st != IDL_STATUS_OK)
            return st;

        if (index >= actual_type->data.record.fields_len) {
            return IDL_STATUS_ERR_INVALID_ARG;
        }

        idl_field      *type_field = &actual_type->data.record.fields[index];
        idl_value_field field;
        field.label = type_field->label;

        st = idl_deserializer_read_value_by_type(de, type_field->type,
                                                 &field.value);
        if (st != IDL_STATUS_OK)
            return st;

        *out = idl_value_variant(de->arena, index, &field);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

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
