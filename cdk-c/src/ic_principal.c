// Principal type implementation
#include "ic_principal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Base32 alphabet (RFC 4648)
static const char BASE32_ALPHABET[] = "abcdefghijklmnopqrstuvwxyz234567";

ic_result_t ic_principal_from_bytes(ic_principal_t *principal,
                                    const uint8_t *bytes, size_t len) {
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

int ic_principal_to_text(const ic_principal_t *principal, char *text,
                         size_t text_len) {
    if (principal == NULL || text == NULL ||
        !ic_principal_is_valid(principal)) {
        return -1;
    }

    // Base32 encoding requires approximately len * 8 / 5 characters
    // Plus some overhead for padding and null terminator
    size_t required_len =
        (principal->len * 8 + 4) / 5 + 10;  // Add padding for safety
    if (text_len < required_len) {
        return -1;
    }

    // Simple base32 encoding (simplified version)
    // For full implementation, should use proper base32 encoding
    // This is a placeholder - full implementation would be more complex
    size_t pos = 0;
    for (size_t i = 0; i < principal->len && pos < text_len - 1; i++) {
        uint8_t byte = principal->bytes[i];
        if (pos + 2 < text_len - 1) {
            text[pos++] = BASE32_ALPHABET[(byte >> 3) & 0x1F];
            text[pos++] = BASE32_ALPHABET[((byte << 2) & 0x1C) |
                                          ((i + 1 < principal->len)
                                               ? (principal->bytes[i + 1] >> 6)
                                               : 0)];
        }
    }

    text[pos] = '\0';
    return (int)pos;
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
