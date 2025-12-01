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
static const idl_type *resolve_type(const idl_type_env *env, const idl_type *type) {
    if (!type) {
        return NULL;
    }
    if (type->kind == IDL_KIND_VAR && env) {
        return idl_type_env_trace(env, type);
    }
    return type;
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

    /* Same type pointer, no coercion needed */
    if (wt == et) {
        *out = (idl_value *)value;
        return IDL_STATUS_OK;
    }

    /* For primitive types, same kind means same type */
    if (wt->kind == et->kind && idl_type_is_primitive(wt)) {
        *out = (idl_value *)value;
        return IDL_STATUS_OK;
    }

    /* reserved: discard the value */
    if (et->kind == IDL_KIND_RESERVED) {
        *out = idl_value_reserved(arena);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    /* nat -> int coercion */
    if (wt->kind == IDL_KIND_NAT && et->kind == IDL_KIND_INT) {
        /* The value is already in LEB128 format, just change the kind */
        idl_value *v = idl_arena_alloc(arena, sizeof(idl_value));
        if (!v)
            return IDL_STATUS_ERR_ALLOC;
        *v = *value;
        v->kind = IDL_VALUE_INT;
        *out = v;
        return IDL_STATUS_OK;
    }

    /* null -> opt T */
    if (wt->kind == IDL_KIND_NULL && et->kind == IDL_KIND_OPT) {
        *out = idl_value_opt_none(arena);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    /* opt T1 -> opt T2: coerce inner value */
    if (wt->kind == IDL_KIND_OPT && et->kind == IDL_KIND_OPT) {
        if (value->kind == IDL_VALUE_OPT && value->data.opt == NULL) {
            /* None stays None */
            *out = idl_value_opt_none(arena);
            return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
        }

        if (value->kind == IDL_VALUE_OPT && value->data.opt != NULL) {
            /* Coerce inner value */
            idl_value *coerced_inner;
            idl_status st = coerce_impl(arena, env, wt->data.inner, et->data.inner, value->data.opt,
                                        &coerced_inner);
            if (st != IDL_STATUS_OK) {
                return st;
            }
            *out = idl_value_opt_some(arena, coerced_inner);
            return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
        }
    }

    /* T -> opt T2: wrap in Some if T <: T2 */
    if (et->kind == IDL_KIND_OPT) {
        const idl_type *inner_et = resolve_type(env, et->data.inner);
        if (inner_et && !idl_type_is_optional_like(env, inner_et)) {
            /* Try to coerce value to inner type */
            idl_value *coerced_inner;
            idl_status st = coerce_impl(arena, env, wt, et->data.inner, value, &coerced_inner);
            if (st == IDL_STATUS_OK) {
                *out = idl_value_opt_some(arena, coerced_inner);
                return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
            }
        }

        /* Special opt rule: coerce to None */
        *out = idl_value_opt_none(arena);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    /* vec T1 -> vec T2: coerce each element */
    if (wt->kind == IDL_KIND_VEC && et->kind == IDL_KIND_VEC) {
        if (value->kind == IDL_VALUE_BLOB) {
            /* blob (vec nat8) - check if target is also vec nat8 */
            const idl_type *inner_et = resolve_type(env, et->data.inner);
            if (inner_et && inner_et->kind == IDL_KIND_NAT8) {
                *out = (idl_value *)value;
                return IDL_STATUS_OK;
            }
            /* Otherwise, need to convert blob to vec */
            idl_value **items = idl_arena_alloc(arena, sizeof(idl_value *) * value->data.blob.len);
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

        if (value->kind == IDL_VALUE_VEC) {
            idl_value **items = idl_arena_alloc(arena, sizeof(idl_value *) * value->data.vec.len);
            if (!items && value->data.vec.len > 0) {
                return IDL_STATUS_ERR_ALLOC;
            }

            for (size_t i = 0; i < value->data.vec.len; i++) {
                idl_status st = coerce_impl(arena, env, wt->data.inner, et->data.inner,
                                            value->data.vec.items[i], &items[i]);
                if (st != IDL_STATUS_OK) {
                    return st;
                }
            }

            *out = idl_value_vec(arena, items, value->data.vec.len);
            return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
        }
    }

    /* record coercion */
    if (wt->kind == IDL_KIND_RECORD && et->kind == IDL_KIND_RECORD) {
        if (value->kind != IDL_VALUE_RECORD) {
            return IDL_STATUS_ERR_INVALID_ARG;
        }

        size_t           expected_count = et->data.record.fields_len;
        idl_value_field *fields = idl_arena_alloc(arena, sizeof(idl_value_field) * expected_count);
        if (!fields && expected_count > 0) {
            return IDL_STATUS_ERR_ALLOC;
        }

        for (size_t i = 0; i < expected_count; i++) {
            idl_field *ef = &et->data.record.fields[i];
            fields[i].label = ef->label;

            /* Find matching field in wire value */
            int found = 0;
            for (size_t j = 0; j < value->data.record.len; j++) {
                if (value->data.record.fields[j].label.id == ef->label.id) {
                    /* Find matching type in wire type */
                    const idl_type *wire_field_type = NULL;
                    for (size_t k = 0; k < wt->data.record.fields_len; k++) {
                        if (wt->data.record.fields[k].label.id == ef->label.id) {
                            wire_field_type = wt->data.record.fields[k].type;
                            break;
                        }
                    }

                    if (wire_field_type) {
                        idl_status st =
                            coerce_impl(arena, env, wire_field_type, ef->type,
                                        value->data.record.fields[j].value, &fields[i].value);
                        if (st != IDL_STATUS_OK) {
                            return st;
                        }
                        found = 1;
                    }
                    break;
                }
            }

            if (!found) {
                /* Field not in wire value; must be optional-like */
                if (idl_type_is_optional_like(env, ef->type)) {
                    const idl_type *ft = resolve_type(env, ef->type);
                    if (ft->kind == IDL_KIND_OPT) {
                        fields[i].value = idl_value_opt_none(arena);
                    } else if (ft->kind == IDL_KIND_NULL) {
                        fields[i].value = idl_value_null(arena);
                    } else {
                        fields[i].value = idl_value_reserved(arena);
                    }
                    if (!fields[i].value) {
                        return IDL_STATUS_ERR_ALLOC;
                    }
                } else {
                    return IDL_STATUS_ERR_INVALID_ARG;
                }
            }
        }

        *out = idl_value_record(arena, fields, expected_count);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    /* variant coercion */
    if (wt->kind == IDL_KIND_VARIANT && et->kind == IDL_KIND_VARIANT) {
        if (value->kind != IDL_VALUE_VARIANT) {
            return IDL_STATUS_ERR_INVALID_ARG;
        }

        /* Find the variant field in expected type */
        uint32_t wire_label_id = value->data.record.fields[0].label.id;

        for (size_t i = 0; i < et->data.record.fields_len; i++) {
            if (et->data.record.fields[i].label.id == wire_label_id) {
                /* Find matching type in wire type */
                const idl_type *wire_field_type = NULL;
                for (size_t k = 0; k < wt->data.record.fields_len; k++) {
                    if (wt->data.record.fields[k].label.id == wire_label_id) {
                        wire_field_type = wt->data.record.fields[k].type;
                        break;
                    }
                }

                if (!wire_field_type) {
                    return IDL_STATUS_ERR_INVALID_ARG;
                }

                idl_value_field field;
                field.label = et->data.record.fields[i].label;

                idl_status st =
                    coerce_impl(arena, env, wire_field_type, et->data.record.fields[i].type,
                                value->data.record.fields[0].value, &field.value);
                if (st != IDL_STATUS_OK) {
                    return st;
                }

                *out = idl_value_variant(arena, i, &field);
                return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
            }
        }

        /* Variant field not found in expected type */
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    /* No coercion possible, return as-is if types match */
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

    switch (wt->kind) {
    case IDL_KIND_NULL:
    case IDL_KIND_RESERVED:
        /* No bytes to skip */
        break;

    case IDL_KIND_BOOL:
    case IDL_KIND_NAT8:
    case IDL_KIND_INT8:
        if (*pos + 1 > data_len)
            return IDL_STATUS_ERR_TRUNCATED;
        (*pos)++;
        break;

    case IDL_KIND_NAT16:
    case IDL_KIND_INT16:
        if (*pos + 2 > data_len)
            return IDL_STATUS_ERR_TRUNCATED;
        *pos += 2;
        break;

    case IDL_KIND_NAT32:
    case IDL_KIND_INT32:
    case IDL_KIND_FLOAT32:
        if (*pos + 4 > data_len)
            return IDL_STATUS_ERR_TRUNCATED;
        *pos += 4;
        break;

    case IDL_KIND_NAT64:
    case IDL_KIND_INT64:
    case IDL_KIND_FLOAT64:
        if (*pos + 8 > data_len)
            return IDL_STATUS_ERR_TRUNCATED;
        *pos += 8;
        break;

    case IDL_KIND_NAT:
    case IDL_KIND_INT: {
        /* Skip LEB128 */
        while (*pos < data_len) {
            uint8_t byte = data[(*pos)++];
            if ((byte & 0x80) == 0) {
                break;
            }
        }
        break;
    }

    case IDL_KIND_TEXT:
    case IDL_KIND_PRINCIPAL: {
        /* Length-prefixed */
        if (wt->kind == IDL_KIND_PRINCIPAL) {
            /* Skip flag byte */
            if (*pos >= data_len)
                return IDL_STATUS_ERR_TRUNCATED;
            (*pos)++;
        }

        uint64_t   len;
        size_t     consumed;
        idl_status st = idl_uleb128_decode(data + *pos, data_len - *pos, &consumed, &len);
        if (st != IDL_STATUS_OK)
            return st;
        *pos += consumed;

        if (*pos + len > data_len)
            return IDL_STATUS_ERR_TRUNCATED;
        *pos += (size_t)len;
        break;
    }

    case IDL_KIND_OPT: {
        if (*pos >= data_len)
            return IDL_STATUS_ERR_TRUNCATED;
        uint8_t flag = data[(*pos)++];
        if (flag == 1) {
            size_t     inner_skipped;
            idl_status st =
                idl_skip_value(data, data_len, pos, env, wt->data.inner, &inner_skipped);
            if (st != IDL_STATUS_OK)
                return st;
        }
        break;
    }

    case IDL_KIND_VEC: {
        uint64_t   len;
        size_t     consumed;
        idl_status st = idl_uleb128_decode(data + *pos, data_len - *pos, &consumed, &len);
        if (st != IDL_STATUS_OK)
            return st;
        *pos += consumed;

        /* Check if blob (vec nat8) */
        const idl_type *inner = resolve_type(env, wt->data.inner);
        if (inner && inner->kind == IDL_KIND_NAT8) {
            if (*pos + len > data_len)
                return IDL_STATUS_ERR_TRUNCATED;
            *pos += (size_t)len;
        } else {
            for (uint64_t i = 0; i < len; i++) {
                size_t elem_skipped;
                st = idl_skip_value(data, data_len, pos, env, wt->data.inner, &elem_skipped);
                if (st != IDL_STATUS_OK)
                    return st;
            }
        }
        break;
    }

    case IDL_KIND_RECORD: {
        for (size_t i = 0; i < wt->data.record.fields_len; i++) {
            size_t     field_skipped;
            idl_status st = idl_skip_value(data, data_len, pos, env, wt->data.record.fields[i].type,
                                           &field_skipped);
            if (st != IDL_STATUS_OK)
                return st;
        }
        break;
    }

    case IDL_KIND_VARIANT: {
        uint64_t   index;
        size_t     consumed;
        idl_status st = idl_uleb128_decode(data + *pos, data_len - *pos, &consumed, &index);
        if (st != IDL_STATUS_OK)
            return st;
        *pos += consumed;

        if (index >= wt->data.record.fields_len)
            return IDL_STATUS_ERR_INVALID_ARG;

        size_t field_skipped;
        st = idl_skip_value(data, data_len, pos, env, wt->data.record.fields[index].type,
                            &field_skipped);
        if (st != IDL_STATUS_OK)
            return st;
        break;
    }

    case IDL_KIND_FUNC:
    case IDL_KIND_SERVICE:
        /* TODO: implement */
        return IDL_STATUS_ERR_UNSUPPORTED;

    default:
        return IDL_STATUS_ERR_UNSUPPORTED;
    }

    if (bytes_skipped) {
        *bytes_skipped = *pos - start_pos;
    }

    return IDL_STATUS_OK;
}
