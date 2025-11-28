#ifndef IDL_VALUE_SERIALIZER_H
#define IDL_VALUE_SERIALIZER_H

#include "arena.h"
#include "base.h"
#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Value serializer: accumulates serialized value bytes.
 * Corresponds to Rust's ValueSerializer.
 */
typedef struct idl_value_serializer {
  idl_arena *arena;
  uint8_t *data;
  size_t len;
  size_t capacity;
} idl_value_serializer;

/*
 * Initialize a value serializer.
 */
idl_status idl_value_serializer_init(idl_value_serializer *ser,
                                     idl_arena *arena);

/*
 * Get the serialized data.
 */
const uint8_t *idl_value_serializer_data(const idl_value_serializer *ser);
size_t idl_value_serializer_len(const idl_value_serializer *ser);

/*
 * Low-level write functions.
 */
idl_status idl_value_serializer_write(idl_value_serializer *ser,
                                      const uint8_t *data, size_t len);
idl_status idl_value_serializer_write_byte(idl_value_serializer *ser,
                                           uint8_t byte);
idl_status idl_value_serializer_write_leb128(idl_value_serializer *ser,
                                             uint64_t value);
idl_status idl_value_serializer_write_sleb128(idl_value_serializer *ser,
                                              int64_t value);

/*
 * Primitive value serialization.
 */
idl_status idl_value_serializer_write_null(idl_value_serializer *ser);
idl_status idl_value_serializer_write_bool(idl_value_serializer *ser, int v);
idl_status idl_value_serializer_write_nat8(idl_value_serializer *ser,
                                           uint8_t v);
idl_status idl_value_serializer_write_nat16(idl_value_serializer *ser,
                                            uint16_t v);
idl_status idl_value_serializer_write_nat32(idl_value_serializer *ser,
                                            uint32_t v);
idl_status idl_value_serializer_write_nat64(idl_value_serializer *ser,
                                            uint64_t v);
idl_status idl_value_serializer_write_int8(idl_value_serializer *ser, int8_t v);
idl_status idl_value_serializer_write_int16(idl_value_serializer *ser,
                                            int16_t v);
idl_status idl_value_serializer_write_int32(idl_value_serializer *ser,
                                            int32_t v);
idl_status idl_value_serializer_write_int64(idl_value_serializer *ser,
                                            int64_t v);
idl_status idl_value_serializer_write_float32(idl_value_serializer *ser,
                                              float v);
idl_status idl_value_serializer_write_float64(idl_value_serializer *ser,
                                              double v);
idl_status idl_value_serializer_write_text(idl_value_serializer *ser,
                                           const char *s, size_t len);
idl_status idl_value_serializer_write_blob(idl_value_serializer *ser,
                                           const uint8_t *data, size_t len);
idl_status idl_value_serializer_write_principal(idl_value_serializer *ser,
                                                const uint8_t *data,
                                                size_t len);
idl_status idl_value_serializer_write_reserved(idl_value_serializer *ser);

/*
 * Composite value serialization.
 */
idl_status idl_value_serializer_write_opt_none(idl_value_serializer *ser);
idl_status idl_value_serializer_write_opt_some(idl_value_serializer *ser);
idl_status idl_value_serializer_write_vec_len(idl_value_serializer *ser,
                                              size_t len);
idl_status idl_value_serializer_write_variant_index(idl_value_serializer *ser,
                                                    uint64_t index);

/*
 * Serialize an idl_value (recursive).
 */
idl_status idl_value_serializer_write_value(idl_value_serializer *ser,
                                            const idl_value *value);

/* Nat/Int arbitrary precision (writes raw LEB128/SLEB128 bytes) */
idl_status idl_value_serializer_write_nat(idl_value_serializer *ser,
                                          const uint8_t *leb_data, size_t len);
idl_status idl_value_serializer_write_int(idl_value_serializer *ser,
                                          const uint8_t *sleb_data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* IDL_VALUE_SERIALIZER_H */
