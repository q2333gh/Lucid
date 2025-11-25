#include <criterion/criterion.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ic_buffer.h"

// Basic lifecycle should allocate on demand and cleanly release memory.
Test(ic_buffer, init_reserve_and_free) {
    ic_buffer_t buf;
    cr_expect_eq(ic_buffer_init(&buf), IC_OK);
    cr_expect_null(buf.data);
    cr_expect_eq(buf.capacity, 0);
    cr_expect_eq(buf.size, 0);

    cr_expect_eq(ic_buffer_reserve(&buf, 32), IC_OK);
    cr_expect_geq(buf.capacity, 32);
    cr_expect_eq(buf.size, 0);

    ic_buffer_free(&buf);
    cr_expect_null(buf.data);
    cr_expect_eq(buf.capacity, 0);
    cr_expect_eq(buf.size, 0);
}

// Clearing should reset size without shrinking the underlying allocation.
Test(ic_buffer, append_and_clear_preserve_capacity) {
    ic_buffer_t buf;
    cr_expect_eq(ic_buffer_init(&buf), IC_OK);

    const uint8_t payload_a[] = {1, 2, 3, 4};
    cr_expect_eq(ic_buffer_append(&buf, payload_a, sizeof(payload_a)), IC_OK);
    cr_expect_eq(buf.size, sizeof(payload_a));
    cr_expect_eq(memcmp(buf.data, payload_a, sizeof(payload_a)), 0);

    const uint8_t payload_b[] = {5, 6, 7};
    cr_expect_eq(ic_buffer_append(&buf, payload_b, sizeof(payload_b)), IC_OK);
    cr_expect_eq(buf.size, sizeof(payload_a) + sizeof(payload_b));
    cr_expect_eq(
        memcmp(buf.data + sizeof(payload_a), payload_b, sizeof(payload_b)), 0);

    size_t previous_capacity = buf.capacity;
    ic_buffer_clear(&buf);
    cr_expect_eq(buf.size, 0);
    cr_expect_eq(buf.capacity, previous_capacity);
    cr_expect_not_null(buf.data);

    ic_buffer_free(&buf);
}

// Byte-wise append helper must behave exactly like the bulk append logic.
Test(ic_buffer, append_byte_is_consistent) {
    ic_buffer_t buf;
    cr_expect_eq(ic_buffer_init(&buf), IC_OK);

    cr_expect_eq(ic_buffer_append_byte(&buf, 0xAA), IC_OK);
    cr_expect_eq(ic_buffer_append_byte(&buf, 0xBB), IC_OK);
    cr_expect_eq(buf.size, 2);
    cr_expect_eq(buf.data[0], 0xAA);
    cr_expect_eq(buf.data[1], 0xBB);

    ic_buffer_free(&buf);
}

// API must defend against null inputs to avoid crashes in host code.
Test(ic_buffer, invalid_arguments_are_rejected) {
    ic_buffer_t buf;
    cr_expect_eq(ic_buffer_init(&buf), IC_OK);

    uint8_t value = 0x42;
    cr_expect_eq(ic_buffer_append(NULL, &value, 1), IC_ERR_INVALID_ARG);
    cr_expect_eq(ic_buffer_append(&buf, NULL, 1), IC_ERR_INVALID_ARG);
    cr_expect_eq(ic_buffer_reserve(NULL, 16), IC_ERR_INVALID_ARG);

    ic_buffer_free(&buf);
}

// Guard against size_t overflow so reserve/append never wrap and
// under-allocate.
Test(ic_buffer, detects_size_overflow_before_allocating) {
    ic_buffer_t buf;
    cr_expect_eq(ic_buffer_init(&buf), IC_OK);

    buf.size = SIZE_MAX - 1;  // Simulate pathological state
    uint8_t value = 0xFF;
    cr_expect_eq(ic_buffer_append(&buf, &value, 16), IC_ERR_BUFFER_OVERFLOW);

    ic_buffer_free(&buf);
}
