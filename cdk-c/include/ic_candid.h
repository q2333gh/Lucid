// Candid binary format encoding/decoding utilities
// Implements serialization for primitive types: text, nat, int, blob,
// principal, vec
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ic_buffer.h"
#include "ic_principal.h"
#include "ic_types.h"

// Type identifier constants (matches Candid specification)
#define CANDID_TYPE_NULL 0x00
#define CANDID_TYPE_BOOL 0x01
#define CANDID_TYPE_NAT 0x02
#define CANDID_TYPE_INT 0x03
#define CANDID_TYPE_NAT8 0x04
#define CANDID_TYPE_INT8 0x05
#define CANDID_TYPE_NAT16 0x06
#define CANDID_TYPE_INT16 0x07
#define CANDID_TYPE_NAT32 0x08
#define CANDID_TYPE_INT32 0x09
#define CANDID_TYPE_NAT64 0x0A
#define CANDID_TYPE_INT64 0x0B
#define CANDID_TYPE_FLOAT32 0x0C
#define CANDID_TYPE_FLOAT64 0x0D
#define CANDID_TYPE_TEXT 0x0D
#define CANDID_TYPE_RESERVED 0x0E
#define CANDID_TYPE_EMPTY 0x0F
#define CANDID_TYPE_OPT 0x10
#define CANDID_TYPE_VEC 0x11
#define CANDID_TYPE_BLOB 0x12
#define CANDID_TYPE_RECORD 0x13
#define CANDID_TYPE_VARIANT 0x14
#define CANDID_TYPE_FUNC 0x15
#define CANDID_TYPE_SERVICE 0x16
#define CANDID_TYPE_PRINCIPAL 0x18

// Candid format header signature (DIDL)
#define CANDID_MAGIC_SIZE 4
static const uint8_t CANDID_MAGIC[] = {'D', 'I', 'D', 'L'};

// Encoding functions (convert C values to Candid binary)
ic_result_t candid_serialize_text(ic_buffer_t *buf, const char *text);
ic_result_t candid_serialize_nat(ic_buffer_t *buf, uint64_t value);
ic_result_t candid_serialize_int(ic_buffer_t *buf, int64_t value);
ic_result_t candid_serialize_blob(ic_buffer_t *buf, const uint8_t *data, size_t len);
ic_result_t candid_serialize_principal(ic_buffer_t *buf, const ic_principal_t *principal);

// Decoding functions (convert Candid binary to C values)
ic_result_t candid_deserialize_text(
    const uint8_t *data, size_t len, size_t *offset, char **text, size_t *text_len);
ic_result_t
candid_deserialize_nat(const uint8_t *data, size_t len, size_t *offset, uint64_t *value);
ic_result_t candid_deserialize_int(const uint8_t *data, size_t len, size_t *offset, int64_t *value);
ic_result_t candid_deserialize_blob(
    const uint8_t *data, size_t len, size_t *offset, uint8_t **blob, size_t *blob_len);
ic_result_t candid_deserialize_principal(const uint8_t  *data,
                                         size_t          len,
                                         size_t         *offset,
                                         ic_principal_t *principal);

// Variable-length integer encoding utilities
// Encode unsigned integer using LEB128 format
ic_result_t candid_write_leb128(ic_buffer_t *buf, uint64_t value);

// Decode unsigned integer from LEB128 format
ic_result_t candid_read_leb128(const uint8_t *data, size_t len, size_t *offset, uint64_t *value);

// Decode signed integer from signed LEB128 format
ic_result_t candid_read_sleb128(const uint8_t *data, size_t len, size_t *offset, int64_t *value);

// Validate Candid format header presence
bool candid_check_magic(const uint8_t *data, size_t len);
