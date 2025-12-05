// Principal type implementation
#include "ic_principal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// =============================================================================
// CRC32 Implementation (ISO 3309)
// =============================================================================

static uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = (crc >> 1);
            }
        }
    }
    return ~crc;
}

// =============================================================================
// Base32 Implementation (RFC 4648)
// =============================================================================
// 1 for human-safe like 1,l
static const char BASE32_ALPHABET[] = "abcdefghijklmnopqrstuvwxyz234567";

// Encode data to Base32 string
static size_t base32_encode(const uint8_t *data, size_t len, char *out) {
    size_t   out_len = 0;
    uint32_t buffer = 0;
    int      bits_left = 0;

    for (size_t i = 0; i < len; i++) {
        buffer = (buffer << 8) | data[i];
        bits_left += 8;
        while (bits_left >= 5) {
            out[out_len++] =
                BASE32_ALPHABET[(buffer >> (bits_left - 5)) & 0x1F];
            bits_left -= 5;
        }
    }

    if (bits_left > 0) {
        out[out_len++] = BASE32_ALPHABET[(buffer << (5 - bits_left)) & 0x1F];
    }

    out[out_len] = '\0';
    return out_len;
}

// =============================================================================
// Principal Logic
// =============================================================================

ic_result_t ic_principal_from_bytes(ic_principal_t *principal,
                                    const uint8_t  *bytes,
                                    size_t          len) {
    if (principal == NULL || bytes == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (len == 0 || len > IC_PRINCIPAL_MAX_LEN) {
        return IC_ERR_INVALID_ARG;
    }

    memcpy(principal->bytes, bytes, len);
    principal->len = len;

    return IC_OK;
}

int ic_principal_to_text(const ic_principal_t *principal,
                         char                 *text,
                         size_t                text_len) {
    if (principal == NULL || text == NULL ||
        !ic_principal_is_valid(principal)) {
        return -1;
    }

    // 1. Calculate CRC32
    uint32_t crc = crc32(principal->bytes, principal->len);

    // 2. Prepare buffer: [CRC32 (4 bytes big endian) | Principal Bytes]
    uint8_t buf[IC_PRINCIPAL_MAX_LEN + 4];
    buf[0] = (crc >> 24) & 0xFF;
    buf[1] = (crc >> 16) & 0xFF;
    buf[2] = (crc >> 8) & 0xFF;
    buf[3] = crc & 0xFF;
    memcpy(buf + 4, principal->bytes, principal->len);

    size_t buf_len = principal->len + 4;

    // 3. Base32 Encode
    // Max base32 length = ceil(33 * 8 / 5) = 53 chars
    char   base32_str[64];
    size_t b32_len = base32_encode(buf, buf_len, base32_str);

    // 4. Format with dashes (xxxxx-xxxxx-...)
    size_t dash_count = 0;
    size_t out_idx = 0;

    for (size_t i = 0; i < b32_len; i++) {
        if (out_idx >= text_len - 1)
            return -1; // Buffer too small

        // Add dash every 5 characters, but not at start or end
        if (i > 0 && i % 5 == 0) {
            text[out_idx++] = '-';
            if (out_idx >= text_len - 1)
                return -1;
        }
        text[out_idx++] = base32_str[i];
    }

    text[out_idx] = '\0';
    return (int)out_idx;
}

bool ic_principal_equal(const ic_principal_t *a, const ic_principal_t *b) {
    if (a == NULL || b == NULL) {
        return false;
    }

    if (a->len != b->len) {
        return false;
    }

    return memcmp(a->bytes, b->bytes, a->len) == 0;
}
