#ifndef IDL_VALUE_H
#define IDL_VALUE_H

#include "arena.h"
#include "base.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDL value kinds (runtime representation of Candid values).
 */
typedef enum idl_value_kind {
    IDL_VALUE_NULL,
    IDL_VALUE_BOOL,
    IDL_VALUE_NAT, /* arbitrary precision, stored as bytes */
    IDL_VALUE_INT, /* arbitrary precision, stored as bytes */
    IDL_VALUE_NAT8,
    IDL_VALUE_NAT16,
    IDL_VALUE_NAT32,
    IDL_VALUE_NAT64,
    IDL_VALUE_INT8,
    IDL_VALUE_INT16,
    IDL_VALUE_INT32,
    IDL_VALUE_INT64,
    IDL_VALUE_FLOAT32,
    IDL_VALUE_FLOAT64,
    IDL_VALUE_TEXT,
    IDL_VALUE_RESERVED,
    IDL_VALUE_PRINCIPAL,
    IDL_VALUE_SERVICE,
    IDL_VALUE_FUNC,
    IDL_VALUE_OPT,     /* optional value */
    IDL_VALUE_VEC,     /* vector of values */
    IDL_VALUE_RECORD,  /* record with named/indexed fields */
    IDL_VALUE_VARIANT, /* variant with single field */
    IDL_VALUE_BLOB,    /* special case: vec nat8 as raw bytes */
} idl_value_kind;

/*
 * Forward declaration for recursive value references.
 */
struct idl_value;

/*
 * Record/variant field value.
 */
typedef struct idl_value_field {
    idl_label         label;
    struct idl_value *value;
} idl_value_field;

/*
 * IDL value: tagged union representing any Candid value.
 */
typedef struct idl_value {
    idl_value_kind kind;

    union {
        /* Primitives */
        int      bool_val;
        uint8_t  nat8_val;
        uint16_t nat16_val;
        uint32_t nat32_val;
        uint64_t nat64_val;
        int8_t   int8_val;
        int16_t  int16_val;
        int32_t  int32_val;
        int64_t  int64_val;
        float    float32_val;
        double   float64_val;

        /* Text */
        struct {
            const char *data;
            size_t      len;
        } text;

        /* Blob (vec nat8) */
        struct {
            const uint8_t *data;
            size_t         len;
        } blob;

        /* Nat/Int (arbitrary precision as LEB128 encoded bytes) */
        struct {
            const uint8_t *data;
            size_t         len;
        } bignum;

        /* Principal */
        struct {
            const uint8_t *data;
            size_t         len;
        } principal;

        /* Service (principal) */
        struct {
            const uint8_t *data;
            size_t         len;
        } service;

        /* Func (principal + method name) */
        struct {
            const uint8_t *principal;
            size_t         principal_len;
            const char    *method;
            size_t         method_len;
        } func;

        /* Opt */
        struct idl_value *opt; /* NULL for None, non-NULL for Some */

        /* Vec */
        struct {
            struct idl_value **items;
            size_t             len;
        } vec;

        /* Record/Variant */
        struct {
            idl_value_field *fields;
            size_t           len;
            uint64_t
                variant_index; /* only for variant: which field is active */
        } record;
    } data;
} idl_value;

/*
 * Value constructors (allocated from arena).
 */
idl_value *idl_value_null(idl_arena *arena);
idl_value *idl_value_bool(idl_arena *arena, int v);
idl_value *idl_value_nat8(idl_arena *arena, uint8_t v);
idl_value *idl_value_nat16(idl_arena *arena, uint16_t v);
idl_value *idl_value_nat32(idl_arena *arena, uint32_t v);
idl_value *idl_value_nat64(idl_arena *arena, uint64_t v);
idl_value *idl_value_int8(idl_arena *arena, int8_t v);
idl_value *idl_value_int16(idl_arena *arena, int16_t v);
idl_value *idl_value_int32(idl_arena *arena, int32_t v);
idl_value *idl_value_int64(idl_arena *arena, int64_t v);
idl_value *idl_value_float32(idl_arena *arena, float v);
idl_value *idl_value_float64(idl_arena *arena, double v);
idl_value *idl_value_text(idl_arena *arena, const char *s, size_t len);
idl_value *idl_value_text_cstr(idl_arena *arena, const char *s);
idl_value *idl_value_blob(idl_arena *arena, const uint8_t *data, size_t len);
idl_value *idl_value_reserved(idl_arena *arena);
idl_value *
idl_value_principal(idl_arena *arena, const uint8_t *data, size_t len);
idl_value *idl_value_service(idl_arena *arena, const uint8_t *data, size_t len);
idl_value *idl_value_func(idl_arena     *arena,
                          const uint8_t *principal,
                          size_t         principal_len,
                          const char    *method,
                          size_t         method_len);
idl_value *idl_value_opt_none(idl_arena *arena);
idl_value *idl_value_opt_some(idl_arena *arena, idl_value *inner);
idl_value *idl_value_vec(idl_arena *arena, idl_value **items, size_t len);
idl_value *
idl_value_record(idl_arena *arena, idl_value_field *fields, size_t len);
idl_value *
idl_value_variant(idl_arena *arena, uint64_t index, idl_value_field *field);

/* Nat/Int with arbitrary precision (LEB128 encoded bytes) */
idl_value *
idl_value_nat_bytes(idl_arena *arena, const uint8_t *leb_data, size_t len);
idl_value *
idl_value_int_bytes(idl_arena *arena, const uint8_t *sleb_data, size_t len);

/* Convenience: Nat/Int from uint64/int64 (will be LEB128 encoded) */
idl_value *idl_value_nat_u64(idl_arena *arena, uint64_t v);
idl_value *idl_value_int_i64(idl_arena *arena, int64_t v);

#ifdef __cplusplus
}
#endif

#endif /* IDL_VALUE_H */
