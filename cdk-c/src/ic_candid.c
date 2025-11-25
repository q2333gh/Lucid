// Candid encoding/decoding implementation
#include "ic_candid.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Encode unsigned integer using variable-length encoding
ic_result_t candid_write_leb128(ic_buffer_t* buf, uint64_t value) {
    if (buf == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    do {
        uint8_t byte = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;  // Continuation bit
        }
        ic_result_t result = ic_buffer_append_byte(buf, byte);
        if (result != IC_OK) {
            return result;
        }
    } while (value != 0);

    return IC_OK;
}

// Decode unsigned integer from variable-length encoding
ic_result_t candid_read_leb128(const uint8_t* data, size_t len, size_t* offset,
                               uint64_t* value) {
    if (data == NULL || offset == NULL || value == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    *value = 0;
    int shift = 0;

    while (*offset < len) {
        if (shift >= 64) {
            return IC_ERR_INVALID_ARG;  // Value exceeds 64 bits
        }

        uint8_t byte = data[*offset];
        (*offset)++;

        *value |= ((uint64_t)(byte & 0x7F)) << shift;

        if ((byte & 0x80) == 0) {
            return IC_OK;  // Encoding complete
        }

        shift += 7;
    }

    return IC_ERR_INVALID_ARG;  // Incomplete encoding
}

// Decode signed integer from variable-length encoding
ic_result_t candid_read_sleb128(const uint8_t* data, size_t len, size_t* offset,
                                int64_t* value) {
    if (data == NULL || offset == NULL || value == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    *value = 0;
    int shift = 0;

    while (*offset < len) {
        if (shift >= 64) {
            return IC_ERR_INVALID_ARG;  // Value exceeds 64 bits
        }

        uint8_t byte = data[*offset];
        (*offset)++;

        *value |= ((int64_t)(byte & 0x7F)) << shift;

        if ((byte & 0x80) == 0) {
            // Apply sign extension when needed
            if (shift < 63 && (byte & 0x40) != 0) {
                int64_t mask = -((int64_t)1 << (shift + 7));
                *value |= mask;
            }
            return IC_OK;  // Encoding complete
        }

        shift += 7;
    }

    return IC_ERR_INVALID_ARG;  // Incomplete encoding
}

bool candid_check_magic(const uint8_t* data, size_t len) {
    if (data == NULL || len < CANDID_MAGIC_SIZE) {
        return false;
    }

    return memcmp(data, CANDID_MAGIC, CANDID_MAGIC_SIZE) == 0;
}

// Encode text string
ic_result_t candid_serialize_text(ic_buffer_t* buf, const char* text) {
    if (buf == NULL || text == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    // Emit type identifier
    ic_result_t result = ic_buffer_append_byte(buf, CANDID_TYPE_TEXT);
    if (result != IC_OK) {
        return result;
    }

    // Emit length prefix
    size_t len = strlen(text);
    result = candid_write_leb128(buf, len);
    if (result != IC_OK) {
        return result;
    }

    // Emit string content
    return ic_buffer_append(buf, (const uint8_t*)text, len);
}

// Encode unsigned integer
ic_result_t candid_serialize_nat(ic_buffer_t* buf, uint64_t value) {
    if (buf == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    // Emit type identifier
    ic_result_t result = ic_buffer_append_byte(buf, CANDID_TYPE_NAT);
    if (result != IC_OK) {
        return result;
    }

    // Emit value using variable-length encoding
    return candid_write_leb128(buf, value);
}

// Encode signed integer
ic_result_t candid_serialize_int(ic_buffer_t* buf, int64_t value) {
    if (buf == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    // Emit type identifier
    ic_result_t result = ic_buffer_append_byte(buf, CANDID_TYPE_INT);
    if (result != IC_OK) {
        return result;
    }

    // Convert to zigzag encoding for signed values
    uint64_t uvalue;
    if (value < 0) {
        uvalue = ((uint64_t)(-value) << 1) | 1;
    } else {
        uvalue = (uint64_t)value << 1;
    }

    return candid_write_leb128(buf, uvalue);
}

// Encode binary data
ic_result_t candid_serialize_blob(ic_buffer_t* buf, const uint8_t* data,
                                  size_t len) {
    if (buf == NULL || data == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    // Write type tag
    ic_result_t result = ic_buffer_append_byte(buf, CANDID_TYPE_BLOB);
    if (result != IC_OK) {
        return result;
    }

    // Write length as LEB128
    result = candid_write_leb128(buf, len);
    if (result != IC_OK) {
        return result;
    }

    // Write blob bytes
    return ic_buffer_append(buf, data, len);
}

// Encode principal identifier
ic_result_t candid_serialize_principal(ic_buffer_t* buf,
                                       const ic_principal_t* principal) {
    if (buf == NULL || principal == NULL || !ic_principal_is_valid(principal)) {
        return IC_ERR_INVALID_ARG;
    }

    // Emit type identifier
    ic_result_t result = ic_buffer_append_byte(buf, CANDID_TYPE_PRINCIPAL);
    if (result != IC_OK) {
        return result;
    }

    // Encode as binary data
    return candid_serialize_blob(buf, principal->bytes, principal->len);
}

// Decode text string
ic_result_t candid_deserialize_text(const uint8_t* data, size_t len,
                                    size_t* offset, char** text,
                                    size_t* text_len) {
    if (data == NULL || offset == NULL || text == NULL || text_len == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (*offset >= len) {
        return IC_ERR_INVALID_ARG;
    }

    // Check type tag
    if (data[*offset] != CANDID_TYPE_TEXT) {
        return IC_ERR_INVALID_ARG;
    }
    (*offset)++;

    // Read length
    uint64_t str_len;
    ic_result_t result = candid_read_leb128(data, len, offset, &str_len);
    if (result != IC_OK) {
        return result;
    }

    if (*offset + str_len > len) {
        return IC_ERR_INVALID_ARG;
    }

    // Allocate and copy text
    *text = (char*)malloc(str_len + 1);
    if (*text == NULL) {
        return IC_ERR_OUT_OF_MEMORY;
    }

    memcpy(*text, data + *offset, str_len);
    (*text)[str_len] = '\0';
    *text_len = str_len;
    *offset += str_len;

    return IC_OK;
}

// Decode unsigned integer
ic_result_t candid_deserialize_nat(const uint8_t* data, size_t len,
                                   size_t* offset, uint64_t* value) {
    if (data == NULL || offset == NULL || value == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (*offset >= len) {
        return IC_ERR_INVALID_ARG;
    }

    // Verify type identifier
    if (data[*offset] != CANDID_TYPE_NAT) {
        return IC_ERR_INVALID_ARG;
    }
    (*offset)++;

    // Decode value
    return candid_read_leb128(data, len, offset, value);
}

// Decode signed integer
ic_result_t candid_deserialize_int(const uint8_t* data, size_t len,
                                   size_t* offset, int64_t* value) {
    if (data == NULL || offset == NULL || value == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (*offset >= len) {
        return IC_ERR_INVALID_ARG;
    }

    // Verify type identifier
    if (data[*offset] != CANDID_TYPE_INT) {
        return IC_ERR_INVALID_ARG;
    }
    (*offset)++;

    // Read encoded value
    uint64_t uvalue;
    ic_result_t result = candid_read_leb128(data, len, offset, &uvalue);
    if (result != IC_OK) {
        return result;
    }

    // Convert from zigzag encoding
    if (uvalue & 1) {
        *value = -(int64_t)(uvalue >> 1);
    } else {
        *value = (int64_t)(uvalue >> 1);
    }

    return IC_OK;
}

// Decode binary data
ic_result_t candid_deserialize_blob(const uint8_t* data, size_t len,
                                    size_t* offset, uint8_t** blob,
                                    size_t* blob_len) {
    if (data == NULL || offset == NULL || blob == NULL || blob_len == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (*offset >= len) {
        return IC_ERR_INVALID_ARG;
    }

    // Verify type identifier
    if (data[*offset] != CANDID_TYPE_BLOB) {
        return IC_ERR_INVALID_ARG;
    }
    (*offset)++;

    // Read length prefix
    uint64_t blob_size;
    ic_result_t result = candid_read_leb128(data, len, offset, &blob_size);
    if (result != IC_OK) {
        return result;
    }

    if (*offset + blob_size > len) {
        return IC_ERR_INVALID_ARG;
    }

    // Allocate storage and copy data content
    *blob = (uint8_t*)malloc(blob_size);
    if (*blob == NULL) {
        return IC_ERR_OUT_OF_MEMORY;
    }

    memcpy(*blob, data + *offset, blob_size);
    *blob_len = blob_size;
    *offset += blob_size;

    return IC_OK;
}

// Decode principal identifier
ic_result_t candid_deserialize_principal(const uint8_t* data, size_t len,
                                         size_t* offset,
                                         ic_principal_t* principal) {
    if (data == NULL || offset == NULL || principal == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (*offset >= len) {
        return IC_ERR_INVALID_ARG;
    }

    // Verify type identifier
    if (data[*offset] != CANDID_TYPE_PRINCIPAL) {
        return IC_ERR_INVALID_ARG;
    }
    (*offset)++;

    // Decode as binary data first
    uint8_t* blob;
    size_t blob_len;
    ic_result_t result =
        candid_deserialize_blob(data, len, offset, &blob, &blob_len);
    if (result != IC_OK) {
        return result;
    }

    // Convert binary data to principal structure
    result = ic_principal_from_bytes(principal, blob, blob_len);
    free(blob);

    return result;
}
