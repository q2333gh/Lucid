#ifndef IDL_TYPES_H
#define IDL_TYPES_H

#include "arena.h"
#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Candid type opcodes (negative SLEB128 values in wire format).
 * Composite types use non-negative indices into the type table.
 */
typedef enum idl_opcode {
    IDL_TYPE_NULL = -1,
    IDL_TYPE_BOOL = -2,
    IDL_TYPE_NAT = -3,
    IDL_TYPE_INT = -4,
    IDL_TYPE_NAT8 = -5,
    IDL_TYPE_NAT16 = -6,
    IDL_TYPE_NAT32 = -7,
    IDL_TYPE_NAT64 = -8,
    IDL_TYPE_INT8 = -9,
    IDL_TYPE_INT16 = -10,
    IDL_TYPE_INT32 = -11,
    IDL_TYPE_INT64 = -12,
    IDL_TYPE_FLOAT32 = -13,
    IDL_TYPE_FLOAT64 = -14,
    IDL_TYPE_TEXT = -15,
    IDL_TYPE_RESERVED = -16,
    IDL_TYPE_EMPTY = -17,
    IDL_TYPE_PRINCIPAL = -24,

    /* Composite type constructors (used in type table) */
    IDL_TYPE_OPT = -18,
    IDL_TYPE_VEC = -19,
    IDL_TYPE_RECORD = -20,
    IDL_TYPE_VARIANT = -21,
    IDL_TYPE_FUNC = -22,
    IDL_TYPE_SERVICE = -23,
} idl_opcode;

/*
 * Label: either a numeric ID or a named field (hashed to ID).
 */
typedef enum idl_label_kind {
    IDL_LABEL_ID,   /* numeric label (e.g., tuple index or explicit id) */
    IDL_LABEL_NAME, /* named label (stores both name and computed hash) */
} idl_label_kind;

typedef struct idl_label {
    idl_label_kind kind;
    uint32_t       id;   /* hash value for NAME, or direct id for ID */
    const char    *name; /* NULL for ID kind */
} idl_label;

/*
 * Forward declaration for recursive type references.
 */
struct idl_type;

/*
 * Field in a record or variant.
 */
typedef struct idl_field {
    idl_label        label;
    struct idl_type *type;
} idl_field;

/*
 * Function mode annotations.
 */
typedef enum idl_func_mode {
    IDL_FUNC_MODE_QUERY = 1,
    IDL_FUNC_MODE_ONEWAY = 2,
    IDL_FUNC_MODE_COMPOSITE_QUERY = 3,
} idl_func_mode;

/*
 * Function type: args, returns, and mode annotations.
 */
typedef struct idl_func {
    struct idl_type **args;
    size_t            args_len;
    struct idl_type **rets;
    size_t            rets_len;
    idl_func_mode    *modes;
    size_t            modes_len;
} idl_func;

/*
 * Service method entry.
 */
typedef struct idl_method {
    const char      *name;
    struct idl_type *type; /* must be a func type */
} idl_method;

/*
 * Service type: list of methods.
 */
typedef struct idl_service {
    idl_method *methods;
    size_t      methods_len;
} idl_service;

/*
 * Type inner representation (tagged union).
 */
typedef enum idl_type_kind {
    /* Primitive types */
    IDL_KIND_NULL,
    IDL_KIND_BOOL,
    IDL_KIND_NAT,
    IDL_KIND_INT,
    IDL_KIND_NAT8,
    IDL_KIND_NAT16,
    IDL_KIND_NAT32,
    IDL_KIND_NAT64,
    IDL_KIND_INT8,
    IDL_KIND_INT16,
    IDL_KIND_INT32,
    IDL_KIND_INT64,
    IDL_KIND_FLOAT32,
    IDL_KIND_FLOAT64,
    IDL_KIND_TEXT,
    IDL_KIND_RESERVED,
    IDL_KIND_EMPTY,
    IDL_KIND_PRINCIPAL,

    /* Composite types */
    IDL_KIND_OPT,     /* opt T */
    IDL_KIND_VEC,     /* vec T */
    IDL_KIND_RECORD,  /* record { fields } */
    IDL_KIND_VARIANT, /* variant { fields } */
    IDL_KIND_FUNC,    /* func signature */
    IDL_KIND_SERVICE, /* service definition */

    /* Reference types (for parsing/building) */
    IDL_KIND_VAR, /* reference to named type in TypeEnv */
} idl_type_kind;

/*
 * The main type structure.
 */
typedef struct idl_type {
    idl_type_kind kind;

    union {
        /* For OPT and VEC */
        struct idl_type *inner;

        /* For RECORD and VARIANT */
        struct {
            idl_field *fields;
            size_t     fields_len;
        } record;

        /* For FUNC */
        idl_func func;

        /* For SERVICE */
        idl_service service;

        /* For VAR (type variable reference) */
        const char *var_name;
    } data;
} idl_type;

/*
 * Check if a type is primitive (doesn't need type table entry).
 */
int idl_type_is_primitive(const idl_type *type);

/*
 * Get the opcode for a type kind.
 */
idl_opcode idl_type_opcode(idl_type_kind kind);

/*
 * Create primitive types (allocated from arena).
 */
idl_type *idl_type_null(idl_arena *arena);
idl_type *idl_type_bool(idl_arena *arena);
idl_type *idl_type_nat(idl_arena *arena);
idl_type *idl_type_int(idl_arena *arena);
idl_type *idl_type_nat8(idl_arena *arena);
idl_type *idl_type_nat16(idl_arena *arena);
idl_type *idl_type_nat32(idl_arena *arena);
idl_type *idl_type_nat64(idl_arena *arena);
idl_type *idl_type_int8(idl_arena *arena);
idl_type *idl_type_int16(idl_arena *arena);
idl_type *idl_type_int32(idl_arena *arena);
idl_type *idl_type_int64(idl_arena *arena);
idl_type *idl_type_float32(idl_arena *arena);
idl_type *idl_type_float64(idl_arena *arena);
idl_type *idl_type_text(idl_arena *arena);
idl_type *idl_type_reserved(idl_arena *arena);
idl_type *idl_type_empty(idl_arena *arena);
idl_type *idl_type_principal(idl_arena *arena);

/*
 * Create composite types.
 */
idl_type *idl_type_opt(idl_arena *arena, idl_type *inner);
idl_type *idl_type_vec(idl_arena *arena, idl_type *inner);
idl_type *idl_type_record(idl_arena *arena, idl_field *fields, size_t len);
idl_type *idl_type_variant(idl_arena *arena, idl_field *fields, size_t len);
idl_type *idl_type_func(idl_arena *arena, const idl_func *func);
idl_type *idl_type_service(idl_arena *arena, const idl_service *service);
idl_type *idl_type_var(idl_arena *arena, const char *name);

/*
 * Label constructors.
 */
idl_label idl_label_id(uint32_t id);
idl_label idl_label_name(const char *name); /* computes hash internally */

#ifdef __cplusplus
}
#endif

#endif /* IDL_TYPES_H */
