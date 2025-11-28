#ifndef IDL_BUILDER_H
#define IDL_BUILDER_H

#include "arena.h"
#include "base.h"
#include "type_table.h"
#include "types.h"
#include "value.h"
#include "value_serializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDL builder: combines type table and value serializer to produce
 * complete DIDL messages.
 * Corresponds to Rust's IDLBuilder.
 */
typedef struct idl_builder {
  idl_arena *arena;
  idl_type_env env;
  idl_type_table_builder type_builder;
  idl_value_serializer value_ser;
} idl_builder;

/*
 * Initialize an IDL builder.
 */
idl_status idl_builder_init(idl_builder *builder, idl_arena *arena);

/*
 * Add an argument with explicit type and value.
 */
idl_status idl_builder_arg(idl_builder *builder, idl_type *type,
                           const idl_value *value);

/*
 * Convenience: add primitive arguments directly.
 */
idl_status idl_builder_arg_null(idl_builder *builder);
idl_status idl_builder_arg_bool(idl_builder *builder, int v);
idl_status idl_builder_arg_nat8(idl_builder *builder, uint8_t v);
idl_status idl_builder_arg_nat16(idl_builder *builder, uint16_t v);
idl_status idl_builder_arg_nat32(idl_builder *builder, uint32_t v);
idl_status idl_builder_arg_nat64(idl_builder *builder, uint64_t v);
idl_status idl_builder_arg_int8(idl_builder *builder, int8_t v);
idl_status idl_builder_arg_int16(idl_builder *builder, int16_t v);
idl_status idl_builder_arg_int32(idl_builder *builder, int32_t v);
idl_status idl_builder_arg_int64(idl_builder *builder, int64_t v);
idl_status idl_builder_arg_float32(idl_builder *builder, float v);
idl_status idl_builder_arg_float64(idl_builder *builder, double v);
idl_status idl_builder_arg_text(idl_builder *builder, const char *s,
                                size_t len);
idl_status idl_builder_arg_text_cstr(idl_builder *builder, const char *s);
idl_status idl_builder_arg_blob(idl_builder *builder, const uint8_t *data,
                                size_t len);
idl_status idl_builder_arg_principal(idl_builder *builder, const uint8_t *data,
                                     size_t len);

/*
 * Serialize the complete DIDL message.
 * Output: "DIDL" + type_table + arg_types + values
 */
idl_status idl_builder_serialize(idl_builder *builder, uint8_t **out,
                                 size_t *out_len);

/*
 * Serialize to hex string (for debugging/testing).
 */
idl_status idl_builder_serialize_hex(idl_builder *builder, char **out,
                                     size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* IDL_BUILDER_H */
