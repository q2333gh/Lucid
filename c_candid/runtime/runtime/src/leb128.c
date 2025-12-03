#include "idl/leb128.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define IDL_CONTINUATION 0x80u
#define IDL_SIGN_BIT 0x40u

static idl_status ensure_capacity(size_t needed, size_t available) {
    return needed <= available ? IDL_STATUS_OK : IDL_STATUS_ERR_TRUNCATED;
}

idl_status idl_uleb128_encode(uint64_t value,
                              uint8_t *out,
                              size_t   out_len,
                              size_t  *written) {
    if (out == NULL) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    size_t count = 0;
    do {
        if (ensure_capacity(count + 1, out_len) != IDL_STATUS_OK) {
            return IDL_STATUS_ERR_TRUNCATED;
        }
        uint8_t byte = (uint8_t)(value & 0x7fu);
        value >>= 7;
        if (value != 0) {
            byte |= IDL_CONTINUATION;
        }
        out[count++] = byte;
    } while (value != 0);
    if (written) {
        *written = count;
    }
    return IDL_STATUS_OK;
}

idl_status idl_sleb128_encode(int64_t  value,
                              uint8_t *out,
                              size_t   out_len,
                              size_t  *written) {
    if (out == NULL) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    size_t count = 0;
    for (;;) {
        if (ensure_capacity(count + 1, out_len) != IDL_STATUS_OK) {
            return IDL_STATUS_ERR_TRUNCATED;
        }
        uint8_t byte = (uint8_t)(value & 0x7f);
        value >>= 6;
        int done = (value == 0 || value == -1);
        if (done) {
            byte &= (uint8_t)~IDL_CONTINUATION;
            out[count++] = byte;
            break;
        } else {
            value >>= 1;
            byte |= IDL_CONTINUATION;
            out[count++] = byte;
        }
    }
    if (written) {
        *written = count;
    }
    return IDL_STATUS_OK;
}

idl_status idl_uleb128_decode(const uint8_t *input,
                              size_t         input_len,
                              size_t        *consumed,
                              uint64_t      *value) {
    if (input == NULL || value == NULL) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    uint64_t result = 0;
    size_t   shift = 0;
    size_t   offset = 0;

    while (offset < input_len) {
        uint8_t  byte = input[offset++];
        uint64_t chunk = (uint64_t)(byte & 0x7fu);
        if (shift >= 64 && chunk != 0) {
            return IDL_STATUS_ERR_OVERFLOW;
        }
        if (shift < 64) {
            uint64_t shifted = chunk << shift;
            if ((shifted >> shift) != chunk) {
                return IDL_STATUS_ERR_OVERFLOW;
            }
            result |= shifted;
        }
        if ((byte & IDL_CONTINUATION) == 0) {
            if (consumed) {
                *consumed = offset;
            }
            *value = result;
            return IDL_STATUS_OK;
        }
        shift += 7;
    }
    return IDL_STATUS_ERR_TRUNCATED;
}

#if defined(__GNUC__) || defined(__clang__)
__extension__ typedef __int128 idl_i128;
#else
#error "Compiler must support __int128 for LEB128 decoding"
#endif

idl_status idl_sleb128_decode(const uint8_t *input,
                              size_t         input_len,
                              size_t        *consumed,
                              int64_t       *value) {
    if (input == NULL || value == NULL) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    idl_i128     result = 0;
    size_t       shift = 0;
    size_t       offset = 0;
    uint8_t      byte = 0;
    const size_t value_bits = 64;

    while (offset < input_len) {
        byte = input[offset++];
        if (shift >= value_bits && byte != 0x00 && byte != 0x7f) {
            return IDL_STATUS_ERR_OVERFLOW;
        }

        idl_i128 low = (idl_i128)(byte & 0x7f);
        result |= low << shift;
        shift += 7;

        if ((byte & IDL_CONTINUATION) == 0) {
            break;
        }
    }

    if ((byte & IDL_CONTINUATION) != 0) {
        return IDL_STATUS_ERR_TRUNCATED;
    }
    const size_t max_bits = sizeof(idl_i128) * CHAR_BIT;
    if ((byte & IDL_SIGN_BIT) == IDL_SIGN_BIT && shift < max_bits) {
        result |= -(((idl_i128)1) << shift);
    }
    if (result > INT64_MAX || result < INT64_MIN) {
        return IDL_STATUS_ERR_OVERFLOW;
    }
    if (consumed) {
        *consumed = offset;
    }
    *value = (int64_t)result;
    return IDL_STATUS_OK;
}
