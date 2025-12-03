/**
 * candid.h - User-friendly type aliases for Candid types
 *
 * This header provides CandidType* style naming that may be more
 * familiar to users coming from other Candid implementations.
 *
 * Usage:
 *   #include "idl/candid.h"
 *
 *   CandidTypeText *t = CandidText(&arena);
 *   CandidTypePrincipal *p = CandidPrincipal(&arena);
 */

#ifndef IDL_CANDID_H
#define IDL_CANDID_H

#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Type Aliases
 * ============================================================ */

/* Base types */
typedef idl_type         CandidType;
typedef idl_value        CandidValue;
typedef idl_arena        CandidArena;
typedef idl_builder      CandidBuilder;
typedef idl_deserializer CandidDeserializer;
typedef idl_field        CandidField;
typedef idl_label        CandidLabel;
typedef idl_status       CandidStatus;

/* Type kinds (for switch statements) */
#define CANDID_TYPE_NULL IDL_KIND_NULL
#define CANDID_TYPE_BOOL IDL_KIND_BOOL
#define CANDID_TYPE_NAT IDL_KIND_NAT
#define CANDID_TYPE_NAT8 IDL_KIND_NAT8
#define CANDID_TYPE_NAT16 IDL_KIND_NAT16
#define CANDID_TYPE_NAT32 IDL_KIND_NAT32
#define CANDID_TYPE_NAT64 IDL_KIND_NAT64
#define CANDID_TYPE_INT IDL_KIND_INT
#define CANDID_TYPE_INT8 IDL_KIND_INT8
#define CANDID_TYPE_INT16 IDL_KIND_INT16
#define CANDID_TYPE_INT32 IDL_KIND_INT32
#define CANDID_TYPE_INT64 IDL_KIND_INT64
#define CANDID_TYPE_FLOAT32 IDL_KIND_FLOAT32
#define CANDID_TYPE_FLOAT64 IDL_KIND_FLOAT64
#define CANDID_TYPE_TEXT IDL_KIND_TEXT
#define CANDID_TYPE_RESERVED IDL_KIND_RESERVED
#define CANDID_TYPE_EMPTY IDL_KIND_EMPTY
#define CANDID_TYPE_PRINCIPAL IDL_KIND_PRINCIPAL
#define CANDID_TYPE_OPT IDL_KIND_OPT
#define CANDID_TYPE_VEC IDL_KIND_VEC
#define CANDID_TYPE_RECORD IDL_KIND_RECORD
#define CANDID_TYPE_VARIANT IDL_KIND_VARIANT

/* Value kinds */
#define CANDID_VALUE_NULL IDL_VALUE_NULL
#define CANDID_VALUE_BOOL IDL_VALUE_BOOL
#define CANDID_VALUE_NAT IDL_VALUE_NAT
#define CANDID_VALUE_NAT8 IDL_VALUE_NAT8
#define CANDID_VALUE_NAT16 IDL_VALUE_NAT16
#define CANDID_VALUE_NAT32 IDL_VALUE_NAT32
#define CANDID_VALUE_NAT64 IDL_VALUE_NAT64
#define CANDID_VALUE_INT IDL_VALUE_INT
#define CANDID_VALUE_INT8 IDL_VALUE_INT8
#define CANDID_VALUE_INT16 IDL_VALUE_INT16
#define CANDID_VALUE_INT32 IDL_VALUE_INT32
#define CANDID_VALUE_INT64 IDL_VALUE_INT64
#define CANDID_VALUE_FLOAT32 IDL_VALUE_FLOAT32
#define CANDID_VALUE_FLOAT64 IDL_VALUE_FLOAT64
#define CANDID_VALUE_TEXT IDL_VALUE_TEXT
#define CANDID_VALUE_RESERVED IDL_VALUE_RESERVED
#define CANDID_VALUE_PRINCIPAL IDL_VALUE_PRINCIPAL
#define CANDID_VALUE_OPT IDL_VALUE_OPT
#define CANDID_VALUE_VEC IDL_VALUE_VEC
#define CANDID_VALUE_RECORD IDL_VALUE_RECORD
#define CANDID_VALUE_VARIANT IDL_VALUE_VARIANT
#define CANDID_VALUE_BLOB IDL_VALUE_BLOB

/* Status codes */
#define CANDID_OK IDL_STATUS_OK
#define CANDID_ERR_ALLOC IDL_STATUS_ERR_ALLOC
#define CANDID_ERR_OVERFLOW IDL_STATUS_ERR_OVERFLOW
#define CANDID_ERR_INVALID_ARG IDL_STATUS_ERR_INVALID_ARG
#define CANDID_ERR_TRUNCATED IDL_STATUS_ERR_TRUNCATED
#define CANDID_ERR_UNSUPPORTED IDL_STATUS_ERR_UNSUPPORTED

/* ============================================================
 * Type Constructors (CandidType* style)
 * ============================================================ */

/* Primitives */
#define CandidTypeNull(arena) idl_type_null(arena)
#define CandidTypeBool(arena) idl_type_bool(arena)
#define CandidTypeNat(arena) idl_type_nat(arena)
#define CandidTypeNat8(arena) idl_type_nat8(arena)
#define CandidTypeNat16(arena) idl_type_nat16(arena)
#define CandidTypeNat32(arena) idl_type_nat32(arena)
#define CandidTypeNat64(arena) idl_type_nat64(arena)
#define CandidTypeInt(arena) idl_type_int(arena)
#define CandidTypeInt8(arena) idl_type_int8(arena)
#define CandidTypeInt16(arena) idl_type_int16(arena)
#define CandidTypeInt32(arena) idl_type_int32(arena)
#define CandidTypeInt64(arena) idl_type_int64(arena)
#define CandidTypeFloat32(arena) idl_type_float32(arena)
#define CandidTypeFloat64(arena) idl_type_float64(arena)
#define CandidTypeText(arena) idl_type_text(arena)
#define CandidTypeReserved(arena) idl_type_reserved(arena)
#define CandidTypeEmpty(arena) idl_type_empty(arena)
#define CandidTypePrincipal(arena) idl_type_principal(arena)

/* Composites */
#define CandidTypeOpt(arena, inner) idl_type_opt(arena, inner)
#define CandidTypeVec(arena, inner) idl_type_vec(arena, inner)
#define CandidTypeRecord(arena, fields, len) idl_type_record(arena, fields, len)
#define CandidTypeVariant(arena, fields, len)                                  \
    idl_type_variant(arena, fields, len)

/* ============================================================
 * Value Constructors (CandidValue* style)
 * ============================================================ */

#define CandidValueNull(arena) idl_value_null(arena)
#define CandidValueBool(arena, v) idl_value_bool(arena, v)
#define CandidValueNat8(arena, v) idl_value_nat8(arena, v)
#define CandidValueNat16(arena, v) idl_value_nat16(arena, v)
#define CandidValueNat32(arena, v) idl_value_nat32(arena, v)
#define CandidValueNat64(arena, v) idl_value_nat64(arena, v)
#define CandidValueInt8(arena, v) idl_value_int8(arena, v)
#define CandidValueInt16(arena, v) idl_value_int16(arena, v)
#define CandidValueInt32(arena, v) idl_value_int32(arena, v)
#define CandidValueInt64(arena, v) idl_value_int64(arena, v)
#define CandidValueFloat32(arena, v) idl_value_float32(arena, v)
#define CandidValueFloat64(arena, v) idl_value_float64(arena, v)
#define CandidValueText(arena, s, len) idl_value_text(arena, s, len)
#define CandidValueTextCStr(arena, s) idl_value_text_cstr(arena, s)
#define CandidValueBlob(arena, data, len) idl_value_blob(arena, data, len)
#define CandidValuePrincipal(arena, d, l) idl_value_principal(arena, d, l)
#define CandidValueOptNone(arena) idl_value_opt_none(arena)
#define CandidValueOptSome(arena, inner) idl_value_opt_some(arena, inner)
#define CandidValueVec(arena, items, len) idl_value_vec(arena, items, len)
#define CandidValueRecord(arena, flds, l) idl_value_record(arena, flds, l)
#define CandidValueVariant(arena, idx, f) idl_value_variant(arena, idx, f)

/* ============================================================
 * Arena Management
 * ============================================================ */

#define CandidArenaInit(arena, size) idl_arena_init(arena, size)
#define CandidArenaDestroy(arena) idl_arena_destroy(arena)
#define CandidArenaAlloc(arena, size) idl_arena_alloc(arena, size)

/* ============================================================
 * Builder API
 * ============================================================ */

#define CandidBuilderInit(b, arena) idl_builder_init(b, arena)
#define CandidBuilderArg(b, type, val) idl_builder_arg(b, type, val)
#define CandidBuilderArgNull(b) idl_builder_arg_null(b)
#define CandidBuilderArgBool(b, v) idl_builder_arg_bool(b, v)
#define CandidBuilderArgNat8(b, v) idl_builder_arg_nat8(b, v)
#define CandidBuilderArgNat16(b, v) idl_builder_arg_nat16(b, v)
#define CandidBuilderArgNat32(b, v) idl_builder_arg_nat32(b, v)
#define CandidBuilderArgNat64(b, v) idl_builder_arg_nat64(b, v)
#define CandidBuilderArgInt8(b, v) idl_builder_arg_int8(b, v)
#define CandidBuilderArgInt16(b, v) idl_builder_arg_int16(b, v)
#define CandidBuilderArgInt32(b, v) idl_builder_arg_int32(b, v)
#define CandidBuilderArgInt64(b, v) idl_builder_arg_int64(b, v)
#define CandidBuilderArgFloat32(b, v) idl_builder_arg_float32(b, v)
#define CandidBuilderArgFloat64(b, v) idl_builder_arg_float64(b, v)
#define CandidBuilderArgText(b, s, l) idl_builder_arg_text(b, s, l)
#define CandidBuilderArgTextCStr(b, s) idl_builder_arg_text_cstr(b, s)
#define CandidBuilderArgBlob(b, d, l) idl_builder_arg_blob(b, d, l)
#define CandidBuilderArgPrincipal(b, d, l) idl_builder_arg_principal(b, d, l)
#define CandidBuilderSerialize(b, o, l) idl_builder_serialize(b, o, l)
#define CandidBuilderSerializeHex(b, o, l) idl_builder_serialize_hex(b, o, l)

/* ============================================================
 * Deserializer API
 * ============================================================ */

#define CandidDeserializerNew(data, len, arena, out)                           \
    idl_deserializer_new(data, len, arena, out)
#define CandidDeserializerGetValue(de, type, val)                              \
    idl_deserializer_get_value(de, type, val)
#define CandidDeserializerIsDone(de) idl_deserializer_is_done(de)
#define CandidDeserializerDone(de) idl_deserializer_done(de)

/* ============================================================
 * Utility
 * ============================================================ */

#define CandidHash(name) idl_hash(name)

#ifdef __cplusplus
}
#endif

#endif /* IDL_CANDID_H */
