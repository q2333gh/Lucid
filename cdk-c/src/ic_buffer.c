// Byte buffer management implementation, like Rust: Vec<u8>
#include "ic_buffer.h"

#include "idl/cdk_alloc.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define IC_BUFFER_INITIAL_CAPACITY 64

ic_result_t ic_buffer_init(ic_buffer_t *buf) {
    if (buf == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    buf->data = NULL;
    buf->capacity = 0;
    buf->size = 0;

    return IC_OK;
}

ic_result_t ic_buffer_reserve(ic_buffer_t *buf, size_t capacity) {
    if (buf == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (capacity <= buf->capacity) {
        return IC_OK;
    }

    // Round up to next power of 2 for better performance
    size_t new_capacity =
        buf->capacity == 0 ? IC_BUFFER_INITIAL_CAPACITY : buf->capacity;
    while (new_capacity < capacity) {
        new_capacity *= 2;
        if (new_capacity < buf->capacity) {
            return IC_ERR_OUT_OF_MEMORY; // Overflow
        }
    }

    uint8_t *new_data = (uint8_t *)realloc(buf->data, new_capacity);
    if (new_data == NULL) {
        return IC_ERR_OUT_OF_MEMORY;
    }

    buf->data = new_data;
    buf->capacity = new_capacity;

    return IC_OK;
}

ic_result_t
ic_buffer_append(ic_buffer_t *buf, const uint8_t *data, size_t len) {
    if (buf == NULL || data == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    if (len == 0) {
        return IC_OK;
    }

    // Check for overflow
    if (buf->size > SIZE_MAX - len) {
        return IC_ERR_BUFFER_OVERFLOW;
    }

    size_t      new_size = buf->size + len;
    ic_result_t result = ic_buffer_reserve(buf, new_size);
    if (result != IC_OK) {
        return result;
    }

    memcpy(buf->data + buf->size, data, len);
    buf->size = new_size;

    return IC_OK;
}

ic_result_t ic_buffer_append_byte(ic_buffer_t *buf, uint8_t byte) {
    return ic_buffer_append(buf, &byte, 1);
}

void ic_buffer_clear(ic_buffer_t *buf) {
    if (buf != NULL) {
        buf->size = 0;
    }
}

void ic_buffer_free(ic_buffer_t *buf) {
    if (buf != NULL) {
        free(buf->data);
        buf->data = NULL;
        buf->capacity = 0;
        buf->size = 0;
    }
}
