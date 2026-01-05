// Principal type handling
#pragma once

#include <stddef.h>

#include "ic_types.h"

// Create principal from byte array
ic_result_t ic_principal_from_bytes(ic_principal_t *principal,
                                    const uint8_t  *bytes,
                                    size_t          len);

// Convert principal to text representation (base32 encoding)
// Returns the length of the text, or -1 on error
// The text buffer must be at least IC_PRINCIPAL_MAX_LEN * 2 + 1 bytes
int ic_principal_to_text(const ic_principal_t *principal,
                         char                 *text,
                         size_t                text_len);

// Compare two principals
bool ic_principal_equal(const ic_principal_t *a, const ic_principal_t *b);

// Check if principal is valid (non-empty)
static inline bool ic_principal_is_valid(const ic_principal_t *principal) {
    return principal != NULL && principal->len > 0 &&
           principal->len <= IC_PRINCIPAL_MAX_LEN;
}

// Parses a principal from its text (base32) representation.
// Returns IC_OK on success or IC_ERROR on failure.
// The text should be a valid null-terminated principal string.
ic_result_t ic_principal_from_text(ic_principal_t *principal, const char *text);
