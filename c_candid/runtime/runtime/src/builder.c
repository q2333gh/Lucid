#include "idl/builder.h"
#include "idl/header.h"
#include <string.h>

idl_status idl_builder_init(idl_builder *builder, idl_arena *arena) {
  if (!builder || !arena) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  builder->arena = arena;

  idl_status st = idl_type_env_init(&builder->env, arena);
  if (st != IDL_STATUS_OK) {
    return st;
  }

  st =
      idl_type_table_builder_init(&builder->type_builder, arena, &builder->env);
  if (st != IDL_STATUS_OK) {
    return st;
  }

  st = idl_value_serializer_init(&builder->value_ser, arena);
  if (st != IDL_STATUS_OK) {
    return st;
  }

  return IDL_STATUS_OK;
}

idl_status idl_builder_arg(idl_builder *builder, idl_type *type,
                           const idl_value *value) {
  if (!builder || !type || !value) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  /* Register the type */
  idl_status st = idl_type_table_builder_push_arg(&builder->type_builder, type);
  if (st != IDL_STATUS_OK) {
    return st;
  }

  /* Serialize the value */
  st = idl_value_serializer_write_value(&builder->value_ser, value);
  if (st != IDL_STATUS_OK) {
    return st;
  }

  return IDL_STATUS_OK;
}

/* Convenience functions for primitive types */

idl_status idl_builder_arg_null(idl_builder *builder) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_null(builder->arena);
  idl_value *value = idl_value_null(builder->arena);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_bool(idl_builder *builder, int v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_bool(builder->arena);
  idl_value *value = idl_value_bool(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_nat8(idl_builder *builder, uint8_t v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_nat8(builder->arena);
  idl_value *value = idl_value_nat8(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_nat16(idl_builder *builder, uint16_t v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_nat16(builder->arena);
  idl_value *value = idl_value_nat16(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_nat32(idl_builder *builder, uint32_t v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_nat32(builder->arena);
  idl_value *value = idl_value_nat32(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_nat64(idl_builder *builder, uint64_t v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_nat64(builder->arena);
  idl_value *value = idl_value_nat64(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_int8(idl_builder *builder, int8_t v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_int8(builder->arena);
  idl_value *value = idl_value_int8(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_int16(idl_builder *builder, int16_t v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_int16(builder->arena);
  idl_value *value = idl_value_int16(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_int32(idl_builder *builder, int32_t v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_int32(builder->arena);
  idl_value *value = idl_value_int32(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_int64(idl_builder *builder, int64_t v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_int64(builder->arena);
  idl_value *value = idl_value_int64(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_float32(idl_builder *builder, float v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_float32(builder->arena);
  idl_value *value = idl_value_float32(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_float64(idl_builder *builder, double v) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_float64(builder->arena);
  idl_value *value = idl_value_float64(builder->arena, v);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_text(idl_builder *builder, const char *s,
                                size_t len) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_text(builder->arena);
  
  idl_value *value = idl_value_text(builder->arena, s, len);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_text_cstr(idl_builder *builder, const char *s) {
  return idl_builder_arg_text(builder, s, strlen(s));
}

idl_status idl_builder_arg_blob(idl_builder *builder, const uint8_t *data,
                                size_t len) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  /* blob is vec nat8 */
  idl_type *inner = idl_type_nat8(builder->arena);
  idl_type *type = idl_type_vec(builder->arena, inner);
  idl_value *value = idl_value_blob(builder->arena, data, len);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_arg_principal(idl_builder *builder, const uint8_t *data,
                                     size_t len) {
  if (!builder) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  idl_type *type = idl_type_principal(builder->arena);
  idl_value *value = idl_value_principal(builder->arena, data, len);
  if (!type || !value) {
    return IDL_STATUS_ERR_ALLOC;
  }

  return idl_builder_arg(builder, type, value);
}

idl_status idl_builder_serialize(idl_builder *builder, uint8_t **out,
                                 size_t *out_len) {
  if (!builder || !out || !out_len) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  /* Get header (DIDL + type table + arg types) */
  uint8_t *header_data;
  size_t header_len;
  idl_status st = idl_header_write(&builder->type_builder, builder->arena,
                                   &header_data, &header_len);
  if (st != IDL_STATUS_OK) {
    return st;
  }

  /* Get value data */
  const uint8_t *value_data = idl_value_serializer_data(&builder->value_ser);
  size_t value_len = idl_value_serializer_len(&builder->value_ser);

  /* Combine header + values */
  size_t total_len = header_len + value_len;
  uint8_t *result = idl_arena_alloc(builder->arena, total_len);
  if (!result) {
    return IDL_STATUS_ERR_ALLOC;
  }

  memcpy(result, header_data, header_len);
  if (value_len > 0) {
    memcpy(result + header_len, value_data, value_len);
  }

  *out = result;
  *out_len = total_len;
  return IDL_STATUS_OK;
}

idl_status idl_builder_serialize_hex(idl_builder *builder, char **out,
                                     size_t *out_len) {
  if (!builder || !out || !out_len) {
    return IDL_STATUS_ERR_INVALID_ARG;
  }

  uint8_t *data;
  size_t len;
  idl_status st = idl_builder_serialize(builder, &data, &len);
  if (st != IDL_STATUS_OK) {
    return st;
  }

  /* Convert to hex string */
  size_t hex_len = len * 2;
  char *hex = idl_arena_alloc(builder->arena, hex_len + 1);
  if (!hex) {
    return IDL_STATUS_ERR_ALLOC;
  }

  static const char hex_chars[] = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    hex[i * 2] = hex_chars[(data[i] >> 4) & 0xF];
    hex[i * 2 + 1] = hex_chars[data[i] & 0xF];
  }
  hex[hex_len] = '\0';

  *out = hex;
  *out_len = hex_len;
  return IDL_STATUS_OK;
}
