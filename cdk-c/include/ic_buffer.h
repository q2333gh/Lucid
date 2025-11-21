// Byte buffer management
#pragma once

#include <stddef.h>

#include "ic_types.h"

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t size;
} ic_buffer_t;

// Initialize a buffer
ic_result_t ic_buffer_init(ic_buffer_t *buf);

// Reserve capacity for the buffer
ic_result_t ic_buffer_reserve(ic_buffer_t *buf, size_t capacity);

// Append data to the buffer
ic_result_t ic_buffer_append(ic_buffer_t *buf, const uint8_t *data, size_t len);

// Append a single byte
ic_result_t ic_buffer_append_byte(ic_buffer_t *buf, uint8_t byte);

// Clear the buffer (does not free memory)
void ic_buffer_clear(ic_buffer_t *buf);

// Free the buffer memory
void ic_buffer_free(ic_buffer_t *buf);

// Get buffer data pointer
static inline const uint8_t *ic_buffer_data(const ic_buffer_t *buf) {
    return buf->data;
}

// Get buffer size
static inline size_t ic_buffer_size(const ic_buffer_t *buf) {
    return buf->size;
}

// Check if buffer is empty
static inline bool ic_buffer_empty(const ic_buffer_t *buf) {
    return buf->size == 0;
}
