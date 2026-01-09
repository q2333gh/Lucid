/*
 * Candid Value Construction
 *
 * Provides factory functions for creating idl_value objects representing
 * Candid values. Supports all Candid types:
 * - Primitives: null, bool, integers (nat/int 8/16/32/64), floats, text
 * - Blobs: Byte arrays (vec nat8)
 * - Principals: IC principal identities
 * - Composites: opt, vec, record, variant
 * - Big integers: nat/int with LEB128 encoding
 *
 * All values are allocated from an arena and contain type information
 * along with the actual data. Used for building values before serialization.
 */
#include "idl/value.h"

#include <string.h>

#include "idl/leb128.h"

/* Helper to allocate and initialize a value */
static idl_value *alloc_value(idl_arena *arena, idl_value_kind kind) {
    idl_value *v = idl_arena_alloc_zeroed(arena, sizeof(idl_value));
    if (v) {
        v->kind = kind;
    }
    return v;
}

idl_value *idl_value_null(idl_arena *arena) {
    return alloc_value(arena, IDL_VALUE_NULL);
}

idl_value *idl_value_bool(idl_arena *arena, int v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_BOOL);
    if (val) {
        val->data.bool_val = v ? 1 : 0;
    }
    return val;
}

idl_value *idl_value_nat8(idl_arena *arena, uint8_t v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_NAT8);
    if (val) {
        val->data.nat8_val = v;
    }
    return val;
}

idl_value *idl_value_nat16(idl_arena *arena, uint16_t v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_NAT16);
    if (val) {
        val->data.nat16_val = v;
    }
    return val;
}

idl_value *idl_value_nat32(idl_arena *arena, uint32_t v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_NAT32);
    if (val) {
        val->data.nat32_val = v;
    }
    return val;
}

idl_value *idl_value_nat64(idl_arena *arena, uint64_t v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_NAT64);
    if (val) {
        val->data.nat64_val = v;
    }
    return val;
}

idl_value *idl_value_int8(idl_arena *arena, int8_t v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_INT8);
    if (val) {
        val->data.int8_val = v;
    }
    return val;
}

idl_value *idl_value_int16(idl_arena *arena, int16_t v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_INT16);
    if (val) {
        val->data.int16_val = v;
    }
    return val;
}

idl_value *idl_value_int32(idl_arena *arena, int32_t v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_INT32);
    if (val) {
        val->data.int32_val = v;
    }
    return val;
}

idl_value *idl_value_int64(idl_arena *arena, int64_t v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_INT64);
    if (val) {
        val->data.int64_val = v;
    }
    return val;
}

idl_value *idl_value_float32(idl_arena *arena, float v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_FLOAT32);
    if (val) {
        val->data.float32_val = v;
    }
    return val;
}

idl_value *idl_value_float64(idl_arena *arena, double v) {
    idl_value *val = alloc_value(arena, IDL_VALUE_FLOAT64);
    if (val) {
        val->data.float64_val = v;
    }
    return val;
}

idl_value *idl_value_text(idl_arena *arena, const char *s, size_t len) {
    idl_value *val = alloc_value(arena, IDL_VALUE_TEXT);
    if (val) {
        char *copy = idl_arena_alloc(arena, len + 1);
        if (!copy) {
            return NULL;
        }
        memcpy(copy, s, len);
        copy[len] = '\0';
        val->data.text.data = copy;
        val->data.text.len = len;
    }
    return val;
}

idl_value *idl_value_text_cstr(idl_arena *arena, const char *s) {
    return idl_value_text(arena, s, strlen(s));
}

idl_value *idl_value_blob(idl_arena *arena, const uint8_t *data, size_t len) {
    idl_value *val = alloc_value(arena, IDL_VALUE_BLOB);
    if (val) {
        uint8_t *copy = idl_arena_alloc(arena, len);
        if (!copy && len > 0) {
            return NULL;
        }
        if (len > 0) {
            memcpy(copy, data, len);
        }
        val->data.blob.data = copy;
        val->data.blob.len = len;
    }
    return val;
}

idl_value *idl_value_reserved(idl_arena *arena) {
    return alloc_value(arena, IDL_VALUE_RESERVED);
}

idl_value *
idl_value_principal(idl_arena *arena, const uint8_t *data, size_t len) {
    idl_value *val = alloc_value(arena, IDL_VALUE_PRINCIPAL);
    if (val) {
        uint8_t *copy = idl_arena_alloc(arena, len);
        if (!copy && len > 0) {
            return NULL;
        }
        if (len > 0) {
            memcpy(copy, data, len);
        }
        val->data.principal.data = copy;
        val->data.principal.len = len;
    }
    return val;
}

idl_value *idl_value_opt_none(idl_arena *arena) {
    idl_value *val = alloc_value(arena, IDL_VALUE_OPT);
    if (val) {
        val->data.opt = NULL;
    }
    return val;
}

idl_value *idl_value_opt_some(idl_arena *arena, idl_value *inner) {
    idl_value *val = alloc_value(arena, IDL_VALUE_OPT);
    if (val) {
        val->data.opt = inner;
    }
    return val;
}

idl_value *idl_value_vec(idl_arena *arena, idl_value **items, size_t len) {
    idl_value *val = alloc_value(arena, IDL_VALUE_VEC);
    if (val) {
        idl_value **copy = idl_arena_alloc(arena, len * sizeof(idl_value *));
        if (!copy && len > 0) {
            return NULL;
        }
        if (len > 0) {
            memcpy(copy, items, len * sizeof(idl_value *));
        }
        val->data.vec.items = copy;
        val->data.vec.len = len;
    }
    return val;
}

idl_value *
idl_value_record(idl_arena *arena, idl_value_field *fields, size_t len) {
    idl_value *val = alloc_value(arena, IDL_VALUE_RECORD);
    if (val) {
        idl_value_field *copy =
            idl_arena_alloc(arena, len * sizeof(idl_value_field));
        if (!copy && len > 0) {
            return NULL;
        }
        if (len > 0) {
            memcpy(copy, fields, len * sizeof(idl_value_field));
        }
        val->data.record.fields = copy;
        val->data.record.len = len;
        val->data.record.variant_index = 0;
    }
    return val;
}

idl_value *
idl_value_variant(idl_arena *arena, uint64_t index, idl_value_field *field) {
    idl_value *val = alloc_value(arena, IDL_VALUE_VARIANT);
    if (val) {
        idl_value_field *copy = idl_arena_alloc(arena, sizeof(idl_value_field));
        if (!copy) {
            return NULL;
        }
        *copy = *field;
        val->data.record.fields = copy;
        val->data.record.len = 1;
        val->data.record.variant_index = index;
    }
    return val;
}

idl_value *
idl_value_nat_bytes(idl_arena *arena, const uint8_t *leb_data, size_t len) {
    idl_value *val = alloc_value(arena, IDL_VALUE_NAT);
    if (val) {
        uint8_t *copy = idl_arena_alloc(arena, len);
        if (!copy && len > 0) {
            return NULL;
        }
        if (len > 0) {
            memcpy(copy, leb_data, len);
        }
        val->data.bignum.data = copy;
        val->data.bignum.len = len;
    }
    return val;
}

idl_value *
idl_value_int_bytes(idl_arena *arena, const uint8_t *sleb_data, size_t len) {
    idl_value *val = alloc_value(arena, IDL_VALUE_INT);
    if (val) {
        uint8_t *copy = idl_arena_alloc(arena, len);
        if (!copy && len > 0) {
            return NULL;
        }
        if (len > 0) {
            memcpy(copy, sleb_data, len);
        }
        val->data.bignum.data = copy;
        val->data.bignum.len = len;
    }
    return val;
}

idl_value *idl_value_nat_u64(idl_arena *arena, uint64_t v) {
    uint8_t buf[10];
    size_t  written;
    if (idl_uleb128_encode(v, buf, sizeof(buf), &written) != IDL_STATUS_OK) {
        return NULL;
    }
    return idl_value_nat_bytes(arena, buf, written);
}

idl_value *idl_value_int_i64(idl_arena *arena, int64_t v) {
    uint8_t buf[10];
    size_t  written;
    if (idl_sleb128_encode(v, buf, sizeof(buf), &written) != IDL_STATUS_OK) {
        return NULL;
    }
    return idl_value_int_bytes(arena, buf, written);
}
