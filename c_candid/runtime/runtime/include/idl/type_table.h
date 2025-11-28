#ifndef IDL_TYPE_TABLE_H
#define IDL_TYPE_TABLE_H

#include "arena.h"
#include "base.h"
#include "type_env.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Type table builder: accumulates composite types and serializes them.
 * Corresponds to Rust's TypeSerialize.
 */
typedef struct idl_type_table_builder {
  idl_arena *arena;
  idl_type_env *env; /* for resolving VAR types */

  /* Type table entries (each is a serialized type definition) */
  uint8_t **entries;
  size_t *entry_lens;
  size_t entries_count;
  size_t entries_capacity;

  /* Map from type pointer to table index (for deduplication) */
  /* Simple linear search for now; can optimize with hash map later */
  idl_type **type_keys;
  int32_t *type_indices;
  size_t type_map_count;
  size_t type_map_capacity;

  /* Argument types */
  idl_type **args;
  size_t args_count;
  size_t args_capacity;
} idl_type_table_builder;

/*
 * Initialize a type table builder.
 */
idl_status idl_type_table_builder_init(idl_type_table_builder *builder,
                                       idl_arena *arena, idl_type_env *env);

/*
 * Add an argument type. This also registers all nested composite types.
 */
idl_status idl_type_table_builder_push_arg(idl_type_table_builder *builder,
                                           idl_type *type);

/*
 * Build the type and register it in the type table if composite.
 * Returns the index (>=0) or opcode (<0) for the type.
 */
idl_status idl_type_table_builder_build_type(idl_type_table_builder *builder,
                                             idl_type *type,
                                             int32_t *out_index);

/*
 * Serialize the complete type table and argument sequence.
 * Output format: [type_count LEB128] [type_entries...] [arg_count LEB128]
 * [arg_indices...]
 */
idl_status idl_type_table_builder_serialize(idl_type_table_builder *builder,
                                            uint8_t **out, size_t *out_len);

/*
 * Get the index or opcode for a type (must have been built already).
 */
idl_status
idl_type_table_builder_encode_type(const idl_type_table_builder *builder,
                                   const idl_type *type, int32_t *out_index);

#ifdef __cplusplus
}
#endif

#endif /* IDL_TYPE_TABLE_H */
