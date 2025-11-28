#include "idl/types.h"
#include "idl/hash.h"
#include <string.h>

/*
 * Check if a type is primitive (doesn't need type table entry).
 */
int idl_type_is_primitive(const idl_type *type) {
  if (!type) {
    return 0;
  }
  switch (type->kind) {
  case IDL_KIND_NULL:
  case IDL_KIND_BOOL:
  case IDL_KIND_NAT:
  case IDL_KIND_INT:
  case IDL_KIND_NAT8:
  case IDL_KIND_NAT16:
  case IDL_KIND_NAT32:
  case IDL_KIND_NAT64:
  case IDL_KIND_INT8:
  case IDL_KIND_INT16:
  case IDL_KIND_INT32:
  case IDL_KIND_INT64:
  case IDL_KIND_FLOAT32:
  case IDL_KIND_FLOAT64:
  case IDL_KIND_TEXT:
  case IDL_KIND_RESERVED:
  case IDL_KIND_EMPTY:
  case IDL_KIND_PRINCIPAL:
    return 1;
  default:
    return 0;
  }
}

/*
 * Get the opcode for a type kind.
 */
idl_opcode idl_type_opcode(idl_type_kind kind) {
  switch (kind) {
  case IDL_KIND_NULL:
    return IDL_TYPE_NULL;
  case IDL_KIND_BOOL:
    return IDL_TYPE_BOOL;
  case IDL_KIND_NAT:
    return IDL_TYPE_NAT;
  case IDL_KIND_INT:
    return IDL_TYPE_INT;
  case IDL_KIND_NAT8:
    return IDL_TYPE_NAT8;
  case IDL_KIND_NAT16:
    return IDL_TYPE_NAT16;
  case IDL_KIND_NAT32:
    return IDL_TYPE_NAT32;
  case IDL_KIND_NAT64:
    return IDL_TYPE_NAT64;
  case IDL_KIND_INT8:
    return IDL_TYPE_INT8;
  case IDL_KIND_INT16:
    return IDL_TYPE_INT16;
  case IDL_KIND_INT32:
    return IDL_TYPE_INT32;
  case IDL_KIND_INT64:
    return IDL_TYPE_INT64;
  case IDL_KIND_FLOAT32:
    return IDL_TYPE_FLOAT32;
  case IDL_KIND_FLOAT64:
    return IDL_TYPE_FLOAT64;
  case IDL_KIND_TEXT:
    return IDL_TYPE_TEXT;
  case IDL_KIND_RESERVED:
    return IDL_TYPE_RESERVED;
  case IDL_KIND_EMPTY:
    return IDL_TYPE_EMPTY;
  case IDL_KIND_PRINCIPAL:
    return IDL_TYPE_PRINCIPAL;
  case IDL_KIND_OPT:
    return IDL_TYPE_OPT;
  case IDL_KIND_VEC:
    return IDL_TYPE_VEC;
  case IDL_KIND_RECORD:
    return IDL_TYPE_RECORD;
  case IDL_KIND_VARIANT:
    return IDL_TYPE_VARIANT;
  case IDL_KIND_FUNC:
    return IDL_TYPE_FUNC;
  case IDL_KIND_SERVICE:
    return IDL_TYPE_SERVICE;
  case IDL_KIND_VAR:
    /* VAR doesn't have an opcode; it references another type */
    return 0;
  }
  return 0;
}

/* Helper to allocate and initialize a type */
static idl_type *alloc_type(idl_arena *arena, idl_type_kind kind) {
  idl_type *t = idl_arena_alloc_zeroed(arena, sizeof(idl_type));
  if (t) {
    t->kind = kind;
  }
  return t;
}

/* Primitive type constructors */
idl_type *idl_type_null(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_NULL);
}

idl_type *idl_type_bool(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_BOOL);
}

idl_type *idl_type_nat(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_NAT);
}

idl_type *idl_type_int(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_INT);
}

idl_type *idl_type_nat8(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_NAT8);
}

idl_type *idl_type_nat16(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_NAT16);
}

idl_type *idl_type_nat32(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_NAT32);
}

idl_type *idl_type_nat64(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_NAT64);
}

idl_type *idl_type_int8(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_INT8);
}

idl_type *idl_type_int16(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_INT16);
}

idl_type *idl_type_int32(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_INT32);
}

idl_type *idl_type_int64(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_INT64);
}

idl_type *idl_type_float32(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_FLOAT32);
}

idl_type *idl_type_float64(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_FLOAT64);
}

idl_type *idl_type_text(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_TEXT);
}

idl_type *idl_type_reserved(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_RESERVED);
}

idl_type *idl_type_empty(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_EMPTY);
}

idl_type *idl_type_principal(idl_arena *arena) {
  return alloc_type(arena, IDL_KIND_PRINCIPAL);
}

/* Composite type constructors */
idl_type *idl_type_opt(idl_arena *arena, idl_type *inner) {
  idl_type *t = alloc_type(arena, IDL_KIND_OPT);
  if (t) {
    t->data.inner = inner;
  }
  return t;
}

idl_type *idl_type_vec(idl_arena *arena, idl_type *inner) {
  idl_type *t = alloc_type(arena, IDL_KIND_VEC);
  if (t) {
    t->data.inner = inner;
  }
  return t;
}

idl_type *idl_type_record(idl_arena *arena, idl_field *fields, size_t len) {
  idl_type *t = alloc_type(arena, IDL_KIND_RECORD);
  if (t) {
    t->data.record.fields = fields;
    t->data.record.fields_len = len;
  }
  return t;
}

idl_type *idl_type_variant(idl_arena *arena, idl_field *fields, size_t len) {
  idl_type *t = alloc_type(arena, IDL_KIND_VARIANT);
  if (t) {
    t->data.record.fields = fields;
    t->data.record.fields_len = len;
  }
  return t;
}

idl_type *idl_type_func(idl_arena *arena, const idl_func *func) {
  idl_type *t = alloc_type(arena, IDL_KIND_FUNC);
  if (t && func) {
    t->data.func = *func;
  }
  return t;
}

idl_type *idl_type_service(idl_arena *arena, const idl_service *service) {
  idl_type *t = alloc_type(arena, IDL_KIND_SERVICE);
  if (t && service) {
    t->data.service = *service;
  }
  return t;
}

idl_type *idl_type_var(idl_arena *arena, const char *name) {
  idl_type *t = alloc_type(arena, IDL_KIND_VAR);
  if (t) {
    t->data.var_name = name;
  }
  return t;
}

/* Label constructors */
idl_label idl_label_id(uint32_t id) {
  idl_label label;
  label.kind = IDL_LABEL_ID;
  label.id = id;
  label.name = NULL;
  return label;
}

idl_label idl_label_name(const char *name) {
  idl_label label;
  label.kind = IDL_LABEL_NAME;
  label.id = idl_hash(name);
  label.name = name;
  return label;
}
