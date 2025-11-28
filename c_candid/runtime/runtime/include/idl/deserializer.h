#ifndef IDL_DESERIALIZER_H
#define IDL_DESERIALIZER_H

#include "arena.h"
#include "base.h"
#include "header.h"
#include "type_env.h"
#include "types.h"
#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decoder configuration for quota and safety limits.
 */
typedef struct idl_decoder_config {
  size_t decoding_quota;  /* 0 means no limit */
  size_t skipping_quota;  /* 0 means no limit */
  int full_error_message; /* whether to include detailed error info */
} idl_decoder_config;

/*
 * Initialize default decoder config (no limits).
 */
void idl_decoder_config_init(idl_decoder_config *config);

/*
 * Set decoding quota (returns self for chaining).
 */
idl_decoder_config *
idl_decoder_config_set_decoding_quota(idl_decoder_config *config, size_t quota);

/*
 * Set skipping quota (returns self for chaining).
 */
idl_decoder_config *
idl_decoder_config_set_skipping_quota(idl_decoder_config *config, size_t quota);

/*
 * IDL deserializer: decodes DIDL binary to values.
 * Corresponds to Rust's IDLDeserialize.
 */
typedef struct idl_deserializer {
  idl_arena *arena;
  idl_decoder_config config;

  /* Input buffer and position */
  const uint8_t *input;
  size_t input_len;
  size_t pos;

  /* Parsed header */
  idl_header header;

  /* Current argument index */
  size_t arg_index;

  /* Cost tracking */
  size_t cost_accumulated;
} idl_deserializer;

/*
 * Create a new deserializer from DIDL binary data.
 */
idl_status idl_deserializer_new(const uint8_t *data, size_t len,
                                idl_arena *arena, idl_deserializer **out);

/*
 * Create a new deserializer with custom config.
 */
idl_status idl_deserializer_new_with_config(const uint8_t *data, size_t len,
                                            idl_arena *arena,
                                            const idl_decoder_config *config,
                                            idl_deserializer **out);

/*
 * Get the next value from the deserializer.
 * Returns the wire type and decoded value.
 */
idl_status idl_deserializer_get_value(idl_deserializer *de, idl_type **out_type,
                                      idl_value **out_value);

/*
 * Get a value with expected type (for subtype coercion).
 */
idl_status idl_deserializer_get_value_with_type(idl_deserializer *de,
                                                const idl_type *expected_type,
                                                idl_value **out_value);

/*
 * Check if all values have been consumed.
 */
int idl_deserializer_is_done(const idl_deserializer *de);

/*
 * Verify no trailing bytes remain.
 */
idl_status idl_deserializer_done(idl_deserializer *de);

/*
 * Get remaining bytes count.
 */
size_t idl_deserializer_remaining(const idl_deserializer *de);

/*
 * Low-level read functions.
 */
idl_status idl_deserializer_read_bytes(idl_deserializer *de, size_t len,
                                       const uint8_t **out);
idl_status idl_deserializer_read_byte(idl_deserializer *de, uint8_t *out);
idl_status idl_deserializer_read_leb128(idl_deserializer *de, uint64_t *out);
idl_status idl_deserializer_read_sleb128(idl_deserializer *de, int64_t *out);

/*
 * Read primitive values.
 */
idl_status idl_deserializer_read_bool(idl_deserializer *de, int *out);
idl_status idl_deserializer_read_nat8(idl_deserializer *de, uint8_t *out);
idl_status idl_deserializer_read_nat16(idl_deserializer *de, uint16_t *out);
idl_status idl_deserializer_read_nat32(idl_deserializer *de, uint32_t *out);
idl_status idl_deserializer_read_nat64(idl_deserializer *de, uint64_t *out);
idl_status idl_deserializer_read_int8(idl_deserializer *de, int8_t *out);
idl_status idl_deserializer_read_int16(idl_deserializer *de, int16_t *out);
idl_status idl_deserializer_read_int32(idl_deserializer *de, int32_t *out);
idl_status idl_deserializer_read_int64(idl_deserializer *de, int64_t *out);
idl_status idl_deserializer_read_float32(idl_deserializer *de, float *out);
idl_status idl_deserializer_read_float64(idl_deserializer *de, double *out);
idl_status idl_deserializer_read_text(idl_deserializer *de, const char **out,
                                      size_t *out_len);
idl_status idl_deserializer_read_blob(idl_deserializer *de, const uint8_t **out,
                                      size_t *out_len);
idl_status idl_deserializer_read_principal(idl_deserializer *de,
                                           const uint8_t **out,
                                           size_t *out_len);

/*
 * Read a value according to its wire type.
 */
idl_status idl_deserializer_read_value_by_type(idl_deserializer *de,
                                               const idl_type *wire_type,
                                               idl_value **out);

/*
 * Add cost to the deserializer (for quota tracking).
 * Returns IDL_STATUS_ERR_OVERFLOW if quota exceeded.
 */
idl_status idl_deserializer_add_cost(idl_deserializer *de, size_t cost);

/*
 * Get the current accumulated cost.
 */
size_t idl_deserializer_get_cost(const idl_deserializer *de);

/*
 * Get the current decoder config.
 */
const idl_decoder_config *
idl_deserializer_get_config(const idl_deserializer *de);

#ifdef __cplusplus
}
#endif

#endif /* IDL_DESERIALIZER_H */
