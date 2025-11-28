#ifndef IDL_COERCE_H
#define IDL_COERCE_H

#include "arena.h"
#include "base.h"
#include "subtype.h"
#include "type_env.h"
#include "types.h"
#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Coerce a value from wire_type to expected_type.
 *
 * This handles:
 * - Skipping extra record fields
 * - Filling missing optional fields with null
 * - Wrapping values in opt when needed
 * - nat -> int coercion
 *
 * @param arena         Arena for allocations.
 * @param env           Type environment.
 * @param wire_type     The type of the value on the wire.
 * @param expected_type The type we want.
 * @param value         The value to coerce.
 * @param out           The coerced value.
 */
idl_status idl_coerce_value(idl_arena *arena, const idl_type_env *env,
                            const idl_type *wire_type,
                            const idl_type *expected_type,
                            const idl_value *value, idl_value **out);

/*
 * Skip a value on the wire (for extra fields).
 * This is used when decoding a record with more fields than expected.
 */
idl_status idl_skip_value(const uint8_t *data, size_t data_len, size_t *pos,
                          const idl_type_env *env, const idl_type *wire_type,
                          size_t *bytes_skipped);

#ifdef __cplusplus
}
#endif

#endif /* IDL_COERCE_H */
