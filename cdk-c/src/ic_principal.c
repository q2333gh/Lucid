// Principal type implementation
#include "ic_principal.h"

#include <ctype.h>
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

// Lookup table for Base32 decoding, initialized on first use
static int    base32_decode_table_initialized = 0;
static int8_t BASE32_DECODE_TABLE[256];

static void init_base32_decode_table() {
    if (base32_decode_table_initialized)
        return;
    memset(BASE32_DECODE_TABLE, -1, sizeof(BASE32_DECODE_TABLE));
    for (int i = 0; i < 32; i++) {
        BASE32_DECODE_TABLE[(uint8_t)BASE32_ALPHABET[i]] = i;
        // Accept uppercase letters as well for robustness
        if (BASE32_ALPHABET[i] >= 'a' && BASE32_ALPHABET[i] <= 'z') {
            BASE32_DECODE_TABLE[(uint8_t)(BASE32_ALPHABET[i] - 'a' + 'A')] = i;
        }
    }
    base32_decode_table_initialized = 1;
}

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

// Decode base32 string (continuous, not with dashes)
// Returns the number of bytes written to out, or -1 on error
static int
base32_decode(const char *in, size_t in_len, uint8_t *out, size_t out_cap) {
    init_base32_decode_table();

    uint32_t buffer = 0;
    int      bits_left = 0;
    size_t   out_len = 0;
    for (size_t i = 0; i < in_len; i++) {
        char ch = in[i];
        if (ch == '\0')
            break;
        if (isspace((unsigned char)ch))
            continue;
        int8_t val = BASE32_DECODE_TABLE[(uint8_t)ch];
        if (val < 0)
            return -1;
        buffer = (buffer << 5) | val;
        bits_left += 5;
        if (bits_left >= 8) {
            bits_left -= 8;
            if (out_len >= out_cap)
                return -1;
            out[out_len++] = (buffer >> bits_left) & 0xFF;
        }
    }
    // Note: RFC 4648 (section 6) says any leftover bits must be 0
    if (bits_left > 0) {
        if ((buffer & ((1 << bits_left) - 1)) != 0)
            return -1;
    }
    return (int)out_len;
}

// Remove dashes from a principal text string (returns length of result)
// dst must be big enough (same length or shorter than src)
static size_t remove_dashes(const char *src, size_t src_len, char *dst) {
    size_t j = 0;
    for (size_t i = 0; i < src_len; i++) {
        if (src[i] != '-') {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
    return j;
}

// =============================================================================
// Principal Logic
// =============================================================================

ic_result_t ic_principal_from_bytes(ic_principal_t *principal,
                                    const uint8_t  *bytes,
                                    size_t          len) {
    if (principal == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    // Length 0 is valid (management canister has empty principal)
    // bytes can be NULL if len is 0
    if ((len > 0 && bytes == NULL) || len > IC_PRINCIPAL_MAX_LEN) {
        return IC_ERR_INVALID_ARG;
    }

    if (len > 0) {
        memcpy(principal->bytes, bytes, len);
    }
    principal->len = len;

    return IC_OK;
}

// New: from text principal string (e.g., "uxrrr-q7777-77774-qaaaq-cai")
ic_result_t ic_principal_from_text(ic_principal_t *principal,
                                   const char     *text) {
    if (principal == NULL || text == NULL) {
        return IC_ERR_INVALID_ARG;
    }
    size_t text_len = strlen(text);
    if (text_len == 0)
        return IC_ERR_INVALID_ARG;

    // Remove dashes
    char   b32_buf[72];
    size_t b32_len = remove_dashes(text, text_len, b32_buf);

    // Check valid length
    if (b32_len < 8 || b32_len > 64)
        return IC_ERR_INVALID_ARG;

    // Decode base32
    uint8_t full_buf[IC_PRINCIPAL_MAX_LEN + 4];
    int buf_len = base32_decode(b32_buf, b32_len, full_buf, sizeof(full_buf));
    if (buf_len < 5)
        return IC_ERR_INVALID_ARG;

    // Split CRC32 (first 4 bytes), then the remaining = principal
    uint32_t crc_in = ((uint32_t)full_buf[0] << 24) |
                      ((uint32_t)full_buf[1] << 16) |
                      ((uint32_t)full_buf[2] << 8) | ((uint32_t)full_buf[3]);

    size_t principal_len = buf_len - 4;
    if (principal_len == 0 || principal_len > IC_PRINCIPAL_MAX_LEN)
        return IC_ERR_INVALID_ARG;

    // Validate CRC32
    uint32_t crc_calc = crc32(full_buf + 4, principal_len);
    if (crc_in != crc_calc)
        return IC_ERR_INVALID_ARG;

    // Store decoded bytes
    memcpy(principal->bytes, full_buf + 4, principal_len);
    principal->len = principal_len;
    return IC_OK;
}

ic_result_t ic_principal_management_canister(ic_principal_t *principal) {
    if (!principal) {
        return IC_ERR_INVALID_ARG;
    }
    // Management canister: "aaaaa-aa" in text form
    // The management canister has an EMPTY principal (0 bytes)
    // This is a special case in the IC protocol
    return ic_principal_from_bytes(principal, NULL, 0);
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
