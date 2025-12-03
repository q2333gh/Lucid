#ifndef IDL_LEB128_H
#define IDL_LEB128_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encode `value` using unsigned LEB128.
 *
 * @param value    Value to encode.
 * @param out      Destination buffer.
 * @param out_len  Length of destination buffer.
 * @param written  Optional pointer to receive number of bytes written.
 */
idl_status idl_uleb128_encode(uint64_t value,
                              uint8_t *out,
                              size_t   out_len,
                              size_t  *written);

/**
 * Encode `value` using signed LEB128.
 */
idl_status idl_sleb128_encode(int64_t  value,
                              uint8_t *out,
                              size_t   out_len,
                              size_t  *written);

/**
 * Decode unsigned LEB128 from buffer.
 *
 * @param consumed Optional pointer to receive number of bytes consumed.
 */
idl_status idl_uleb128_decode(const uint8_t *input,
                              size_t         input_len,
                              size_t        *consumed,
                              uint64_t      *value);

/**
 * Decode signed LEB128 from buffer.
 */
idl_status idl_sleb128_decode(const uint8_t *input,
                              size_t         input_len,
                              size_t        *consumed,
                              int64_t       *value);

#ifdef __cplusplus
}
#endif

#endif /* IDL_LEB128_H */
