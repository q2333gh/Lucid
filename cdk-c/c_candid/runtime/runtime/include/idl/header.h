#ifndef IDL_HEADER_H
#define IDL_HEADER_H

#include "arena.h"
#include "base.h"
#include "type_env.h"
#include "type_table.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DIDL magic bytes: "DIDL" (0x44, 0x49, 0x44, 0x4c)
 */
#define IDL_MAGIC_0 0x44
#define IDL_MAGIC_1 0x49
#define IDL_MAGIC_2 0x44
#define IDL_MAGIC_3 0x4c

/*
 * Parsed DIDL header result.
 */
typedef struct idl_header {
  idl_type_env env;     /* type environment with table entries */
  idl_type **arg_types; /* argument type sequence */
  size_t arg_count;
} idl_header;

/*
 * Parse a DIDL header from binary data.
 *
 * @param data      Input buffer (must start with "DIDL").
 * @param data_len  Length of input buffer.
 * @param arena     Arena for allocations.
 * @param header    Output header structure.
 * @param consumed  Number of bytes consumed (header only, not values).
 */
idl_status idl_header_parse(const uint8_t *data, size_t data_len,
                            idl_arena *arena, idl_header *header,
                            size_t *consumed);

/*
 * Write a DIDL header to a buffer.
 * Writes: "DIDL" + type_table + arg_sequence
 *
 * @param builder   Type table builder with registered types.
 * @param out       Output buffer (allocated from arena).
 * @param out_len   Length of output.
 */
idl_status idl_header_write(idl_type_table_builder *builder, idl_arena *arena,
                            uint8_t **out, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* IDL_HEADER_H */
