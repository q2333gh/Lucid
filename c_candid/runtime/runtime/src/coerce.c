#include "idl/coerce.h"

#include <string.h>

#include "idl/leb128.h"

/* Forward declaration */
static idl_status coerce_impl(idl_arena          *arena,
                              const idl_type_env *env,
                              const idl_type     *wire_type,
                              const idl_type     *expected_type,
                              const idl_value    *value,
                              idl_value         **out);

/* Resolve VAR types */
static const idl_type *resolve_type(const idl_type_env *env,
                                    const idl_type     *type) {
    if (!type) {
        return NULL;
    }
    if (type->kind == IDL_KIND_VAR && env) {
        return idl_type_env_trace(env, type);
    }
    return type;
}

/* Coerce nat to int */
static idl_status
coerce_nat_to_int(idl_arena *arena, const idl_value *value, idl_value **out) {
    idl_value *v = idl_arena_alloc(arena, sizeof(idl_value));
    if (!v) {
        return IDL_STATUS_ERR_ALLOC;
    }
    *v = *value;
    v->kind = IDL_VALUE_INT;
    *out = v;
    return IDL_STATUS_OK;
}

/* Coerce null to opt T */
static idl_status coerce_null_to_opt(idl_arena *arena, idl_value **out) {
    *out = idl_value_opt_none(arena);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Coerce opt T1 to opt T2 */
static idl_status coerce_opt_to_opt(idl_arena          *arena,
                                    const idl_type_env *env,
                                    const idl_type     *wire_inner,
                                    const idl_type     *expected_inner,
                                    const idl_value    *value,
                                    idl_value         **out) {
    if (value->kind == IDL_VALUE_OPT && value->data.opt == NULL) {
        *out = idl_value_opt_none(arena);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    if (value->kind == IDL_VALUE_OPT && value->data.opt != NULL) {
        idl_value *coerced_inner;
        idl_status st = coerce_impl(arena, env, wire_inner, expected_inner,
                                    value->data.opt, &coerced_inner);
        if (st != IDL_STATUS_OK) {
            return st;
        }
        *out = idl_value_opt_some(arena, coerced_inner);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    return IDL_STATUS_ERR_INVALID_ARG;
}

/* Coerce T to opt T2 */
static idl_status coerce_to_opt(idl_arena          *arena,
                                const idl_type_env *env,
                                const idl_type     *wire_type,
                                const idl_type     *expected_inner,
                                const idl_value    *value,
                                idl_value         **out) {
    const idl_type *inner_et = resolve_type(env, expected_inner);
    if (inner_et && !idl_type_is_optional_like(env, inner_et)) {
        idl_value *coerced_inner;
        idl_status st = coerce_impl(arena, env, wire_type, expected_inner,
                                    value, &coerced_inner);
        if (st == IDL_STATUS_OK) {
            *out = idl_value_opt_some(arena, coerced_inner);
            return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
        }
    }

    *out = idl_value_opt_none(arena);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Coerce blob to vec */
static idl_status coerce_blob_to_vec(idl_arena          *arena,
                                     const idl_type_env *env,
                                     const idl_type     *expected_inner,
                                     const idl_value    *value,
                                     idl_value         **out) {
    const idl_type *inner_et = resolve_type(env, expected_inner);
    if (inner_et && inner_et->kind == IDL_KIND_NAT8) {
        *out = (idl_value *)value;
        return IDL_STATUS_OK;
    }

    idl_value **items =
        idl_arena_alloc(arena, sizeof(idl_value *) * value->data.blob.len);
    if (!items && value->data.blob.len > 0) {
        return IDL_STATUS_ERR_ALLOC;
    }
    for (size_t i = 0; i < value->data.blob.len; i++) {
        items[i] = idl_value_nat8(arena, value->data.blob.data[i]);
        if (!items[i]) {
            return IDL_STATUS_ERR_ALLOC;
        }
    }
    *out = idl_value_vec(arena, items, value->data.blob.len);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Coerce vec T1 to vec T2 */
static idl_status coerce_vec_to_vec(idl_arena          *arena,
                                    const idl_type_env *env,
                                    const idl_type     *wire_inner,
                                    const idl_type     *expected_inner,
                                    const idl_value    *value,
                                    idl_value         **out) {
    if (value->kind == IDL_VALUE_BLOB) {
        return coerce_blob_to_vec(arena, env, expected_inner, value, out);
    }

    if (value->kind == IDL_VALUE_VEC) {
        idl_value **items =
            idl_arena_alloc(arena, sizeof(idl_value *) * value->data.vec.len);
        if (!items && value->data.vec.len > 0) {
            return IDL_STATUS_ERR_ALLOC;
        }

        for (size_t i = 0; i < value->data.vec.len; i++) {
            idl_status st = coerce_impl(arena, env, wire_inner, expected_inner,
                                        value->data.vec.items[i], &items[i]);
            if (st != IDL_STATUS_OK) {
                return st;
            }
        }

        *out = idl_value_vec(arena, items, value->data.vec.len);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    return IDL_STATUS_ERR_INVALID_ARG;
}

/* Find wire field type by label */
static const idl_type *find_wire_field_type(const idl_type *wire_type,
                                            uint32_t        label_id) {
    for (size_t k = 0; k < wire_type->data.record.fields_len; k++) {
        if (wire_type->data.record.fields[k].label.id == label_id) {
            return wire_type->data.record.fields[k].type;
        }
    }
    return NULL;
}

/* Coerce record field value */
static idl_status coerce_record_field(idl_arena          *arena,
                                      const idl_type_env *env,
                                      const idl_type     *wire_field_type,
                                      const idl_type     *expected_field_type,
                                      const idl_value    *field_value,
                                      idl_value         **out) {
    return coerce_impl(arena, env, wire_field_type, expected_field_type,
                       field_value, out);
}

/* Create default value for missing optional field */
static idl_status create_default_optional_field(idl_arena          *arena,
                                                const idl_type_env *env,
                                                const idl_type     *field_type,
                                                idl_value         **out) {
    const idl_type *ft = resolve_type(env, field_type);
    if (ft->kind == IDL_KIND_OPT) {
        *out = idl_value_opt_none(arena);
    } else if (ft->kind == IDL_KIND_NULL) {
        *out = idl_value_null(arena);
    } else {
        *out = idl_value_reserved(arena);
    }
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Find matching field value in wire record */
static const idl_value *find_record_field_value(const idl_value *value,
                                                uint32_t         label_id) {
    for (size_t j = 0; j < value->data.record.len; j++) {
        if (value->data.record.fields[j].label.id == label_id) {
            return value->data.record.fields[j].value;
        }
    }
    return NULL;
}

/* Coerce a single record field */
static idl_status coerce_single_record_field(idl_arena          *arena,
                                             const idl_type_env *env,
                                             const idl_type     *wire_type,
                                             const idl_field    *expected_field,
                                             const idl_value    *wire_value,
                                             idl_value         **out) {
    const idl_value *field_value =
        find_record_field_value(wire_value, expected_field->label.id);
    if (field_value) {
        const idl_type *wire_field_type =
            find_wire_field_type(wire_type, expected_field->label.id);
        if (wire_field_type) {
            return coerce_record_field(arena, env, wire_field_type,
                                       expected_field->type, field_value, out);
        }
    }

    if (idl_type_is_optional_like(env, expected_field->type)) {
        return create_default_optional_field(arena, env, expected_field->type,
                                             out);
    }

    return IDL_STATUS_ERR_INVALID_ARG;
}

/* Coerce record to record */
static idl_status coerce_record_to_record(idl_arena          *arena,
                                          const idl_type_env *env,
                                          const idl_type     *wire_type,
                                          const idl_type     *expected_type,
                                          const idl_value    *value,
                                          idl_value         **out) {
    if (value->kind != IDL_VALUE_RECORD) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    size_t           expected_count = expected_type->data.record.fields_len;
    idl_value_field *fields =
        idl_arena_alloc(arena, sizeof(idl_value_field) * expected_count);
    if (!fields && expected_count > 0) {
        return IDL_STATUS_ERR_ALLOC;
    }

    for (size_t i = 0; i < expected_count; i++) {
        idl_field *ef = &expected_type->data.record.fields[i];
        fields[i].label = ef->label;

        idl_status st = coerce_single_record_field(arena, env, wire_type, ef,
                                                   value, &fields[i].value);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    *out = idl_value_record(arena, fields, expected_count);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Coerce variant to variant */
static idl_status coerce_variant_to_variant(idl_arena          *arena,
                                            const idl_type_env *env,
                                            const idl_type     *wire_type,
                                            const idl_type     *expected_type,
                                            const idl_value    *value,
                                            idl_value         **out) {
    if (value->kind != IDL_VALUE_VARIANT) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    uint32_t wire_label_id = value->data.record.fields[0].label.id;

    for (size_t i = 0; i < expected_type->data.record.fields_len; i++) {
        if (expected_type->data.record.fields[i].label.id == wire_label_id) {
            const idl_type *wire_field_type =
                find_wire_field_type(wire_type, wire_label_id);
            if (!wire_field_type) {
                return IDL_STATUS_ERR_INVALID_ARG;
            }

            idl_value_field field;
            field.label = expected_type->data.record.fields[i].label;

            idl_status st =
                coerce_impl(arena, env, wire_field_type,
                            expected_type->data.record.fields[i].type,
                            value->data.record.fields[0].value, &field.value);
            if (st != IDL_STATUS_OK) {
                return st;
            }

            *out = idl_value_variant(arena, i, &field);
            return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
        }
    }

    return IDL_STATUS_ERR_INVALID_ARG;
}

static idl_status coerce_impl(idl_arena          *arena,
                              const idl_type_env *env,
                              const idl_type     *wire_type,
                              const idl_type     *expected_type,
                              const idl_value    *value,
                              idl_value         **out) {
    const idl_type *wt = resolve_type(env, wire_type);
    const idl_type *et = resolve_type(env, expected_type);

    if (!wt || !et) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    if (wt == et) {
        *out = (idl_value *)value;
        return IDL_STATUS_OK;
    }

    if (wt->kind == et->kind && idl_type_is_primitive(wt)) {
        *out = (idl_value *)value;
        return IDL_STATUS_OK;
    }

    if (et->kind == IDL_KIND_RESERVED) {
        *out = idl_value_reserved(arena);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    if (wt->kind == IDL_KIND_NAT && et->kind == IDL_KIND_INT) {
        return coerce_nat_to_int(arena, value, out);
    }

    if (wt->kind == IDL_KIND_NULL && et->kind == IDL_KIND_OPT) {
        return coerce_null_to_opt(arena, out);
    }

    if (wt->kind == IDL_KIND_OPT && et->kind == IDL_KIND_OPT) {
        return coerce_opt_to_opt(arena, env, wt->data.inner, et->data.inner,
                                 value, out);
    }

    if (et->kind == IDL_KIND_OPT) {
        return coerce_to_opt(arena, env, wt, et->data.inner, value, out);
    }

    if (wt->kind == IDL_KIND_VEC && et->kind == IDL_KIND_VEC) {
        return coerce_vec_to_vec(arena, env, wt->data.inner, et->data.inner,
                                 value, out);
    }

    if (wt->kind == IDL_KIND_RECORD && et->kind == IDL_KIND_RECORD) {
        return coerce_record_to_record(arena, env, wt, et, value, out);
    }

    if (wt->kind == IDL_KIND_VARIANT && et->kind == IDL_KIND_VARIANT) {
        return coerce_variant_to_variant(arena, env, wt, et, value, out);
    }

    if (wt->kind == et->kind) {
        *out = (idl_value *)value;
        return IDL_STATUS_OK;
    }

    return IDL_STATUS_ERR_INVALID_ARG;
}

idl_status idl_coerce_value(idl_arena          *arena,
                            const idl_type_env *env,
                            const idl_type     *wire_type,
                            const idl_type     *expected_type,
                            const idl_value    *value,
                            idl_value         **out) {
    if (!arena || !wire_type || !expected_type || !value || !out) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    return coerce_impl(arena, env, wire_type, expected_type, value, out);
}

/* Skip fixed-size types */
static idl_status skip_fixed_size(const uint8_t *data,
                                  size_t         data_len,
                                  size_t        *pos,
                                  size_t         size) {
    if (*pos + size > data_len) {
        return IDL_STATUS_ERR_TRUNCATED;
    }
    *pos += size;
    return IDL_STATUS_OK;
}

/* Skip LEB128 encoded value */
static idl_status
skip_leb128(const uint8_t *data, size_t data_len, size_t *pos) {
    while (*pos < data_len) {
        uint8_t byte = data[(*pos)++];
        if ((byte & 0x80) == 0) {
            break;
        }
    }
    return IDL_STATUS_OK;
}

/* Skip length-prefixed data */
static idl_status skip_length_prefixed(const uint8_t *data,
                                       size_t         data_len,
                                       size_t        *pos,
                                       int            skip_flag_byte) {
    if (skip_flag_byte) {
        if (*pos >= data_len) {
            return IDL_STATUS_ERR_TRUNCATED;
        }
        (*pos)++;
    }

    uint64_t   len;
    size_t     consumed;
    idl_status st =
        idl_uleb128_decode(data + *pos, data_len - *pos, &consumed, &len);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *pos += consumed;

    if (*pos + len > data_len) {
        return IDL_STATUS_ERR_TRUNCATED;
    }
    *pos += (size_t)len;
    return IDL_STATUS_OK;
}

/* Skip optional value */
static idl_status skip_opt_value(const uint8_t      *data,
                                 size_t              data_len,
                                 size_t             *pos,
                                 const idl_type_env *env,
                                 const idl_type     *inner_type) {
    if (*pos >= data_len) {
        return IDL_STATUS_ERR_TRUNCATED;
    }
    uint8_t flag = data[(*pos)++];
    if (flag == 1) {
        size_t     inner_skipped;
        idl_status st = idl_skip_value(data, data_len, pos, env, inner_type,
                                       &inner_skipped);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }
    return IDL_STATUS_OK;
}

/* Skip vector value */
static idl_status skip_vec_value(const uint8_t      *data,
                                 size_t              data_len,
                                 size_t             *pos,
                                 const idl_type_env *env,
                                 const idl_type     *wire_type) {
    uint64_t   len;
    size_t     consumed;
    idl_status st =
        idl_uleb128_decode(data + *pos, data_len - *pos, &consumed, &len);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *pos += consumed;

    const idl_type *inner = resolve_type(env, wire_type->data.inner);
    if (inner && inner->kind == IDL_KIND_NAT8) {
        if (*pos + len > data_len) {
            return IDL_STATUS_ERR_TRUNCATED;
        }
        *pos += (size_t)len;
    } else {
        for (uint64_t i = 0; i < len; i++) {
            size_t elem_skipped;
            st = idl_skip_value(data, data_len, pos, env, wire_type->data.inner,
                                &elem_skipped);
            if (st != IDL_STATUS_OK) {
                return st;
            }
        }
    }
    return IDL_STATUS_OK;
}

/* Skip record value */
static idl_status skip_record_value(const uint8_t      *data,
                                    size_t              data_len,
                                    size_t             *pos,
                                    const idl_type_env *env,
                                    const idl_type     *wire_type) {
    for (size_t i = 0; i < wire_type->data.record.fields_len; i++) {
        size_t     field_skipped;
        idl_status st = idl_skip_value(data, data_len, pos, env,
                                       wire_type->data.record.fields[i].type,
                                       &field_skipped);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }
    return IDL_STATUS_OK;
}

/* Skip variant value */
static idl_status skip_variant_value(const uint8_t      *data,
                                     size_t              data_len,
                                     size_t             *pos,
                                     const idl_type_env *env,
                                     const idl_type     *wire_type) {
    uint64_t   index;
    size_t     consumed;
    idl_status st =
        idl_uleb128_decode(data + *pos, data_len - *pos, &consumed, &index);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    *pos += consumed;

    if (index >= wire_type->data.record.fields_len) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    size_t field_skipped;
    st = idl_skip_value(data, data_len, pos, env,
                        wire_type->data.record.fields[index].type,
                        &field_skipped);
    if (st != IDL_STATUS_OK) {
        return st;
    }
    return IDL_STATUS_OK;
}

/* Skip a value on the wire */
idl_status idl_skip_value(const uint8_t      *data,
                          size_t              data_len,
                          size_t             *pos,
                          const idl_type_env *env,
                          const idl_type     *wire_type,
                          size_t             *bytes_skipped) {
    if (!data || !pos || !wire_type) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    size_t          start_pos = *pos;
    const idl_type *wt = resolve_type(env, wire_type);

    if (!wt) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    idl_status st = IDL_STATUS_OK;

    switch (wt->kind) {
    case IDL_KIND_NULL:
    case IDL_KIND_RESERVED:
        break;

    case IDL_KIND_BOOL:
    case IDL_KIND_NAT8:
    case IDL_KIND_INT8:
        st = skip_fixed_size(data, data_len, pos, 1);
        break;

    case IDL_KIND_NAT16:
    case IDL_KIND_INT16:
        st = skip_fixed_size(data, data_len, pos, 2);
        break;

    case IDL_KIND_NAT32:
    case IDL_KIND_INT32:
    case IDL_KIND_FLOAT32:
        st = skip_fixed_size(data, data_len, pos, 4);
        break;

    case IDL_KIND_NAT64:
    case IDL_KIND_INT64:
    case IDL_KIND_FLOAT64:
        st = skip_fixed_size(data, data_len, pos, 8);
        break;

    case IDL_KIND_NAT:
    case IDL_KIND_INT:
        st = skip_leb128(data, data_len, pos);
        break;

    case IDL_KIND_TEXT:
        st = skip_length_prefixed(data, data_len, pos, 0);
        break;

    case IDL_KIND_PRINCIPAL:
        st = skip_length_prefixed(data, data_len, pos, 1);
        break;

    case IDL_KIND_OPT:
        st = skip_opt_value(data, data_len, pos, env, wt->data.inner);
        break;

    case IDL_KIND_VEC:
        st = skip_vec_value(data, data_len, pos, env, wt);
        break;

    case IDL_KIND_RECORD:
        st = skip_record_value(data, data_len, pos, env, wt);
        break;

    case IDL_KIND_VARIANT:
        st = skip_variant_value(data, data_len, pos, env, wt);
        break;

    case IDL_KIND_FUNC:
    case IDL_KIND_SERVICE:
        return IDL_STATUS_ERR_UNSUPPORTED;

    default:
        return IDL_STATUS_ERR_UNSUPPORTED;
    }

    if (st != IDL_STATUS_OK) {
        return st;
    }

    if (bytes_skipped) {
        *bytes_skipped = *pos - start_pos;
    }

    return IDL_STATUS_OK;
}
