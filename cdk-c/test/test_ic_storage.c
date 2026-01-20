#include <criterion/criterion.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ic_storage.h"

void ic_storage_test_reset(void);

static void fill_pattern(uint8_t *buffer, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }
}

Test(ic_stable_io, write_and_read_roundtrip) {
    ic_storage_test_reset();

    ic_stable_io_t *io = ic_stable_io_create();
    cr_assert_not_null(io);

    size_t   len = IC_STABLE_PAGE_SIZE_BYTES + 128;
    uint8_t *payload = malloc(len);
    uint8_t *readback = malloc(len);
    cr_assert_not_null(payload);
    cr_assert_not_null(readback);

    fill_pattern(payload, len);

    cr_expect_eq(ic_stable_io_write(io, payload, len), IC_STORAGE_OK);
    cr_expect_eq(ic_stable_io_seek(io, 0, IC_STABLE_SEEK_SET), IC_STORAGE_OK);

    int64_t read_len = ic_stable_io_read(io, readback, len);
    cr_expect_eq(read_len, (int64_t)len);
    cr_expect_eq(memcmp(payload, readback, len), 0);

    free(readback);
    free(payload);
    ic_stable_io_free(io);
}

Test(ic_stable_io, seek_end_reports_eof) {
    ic_storage_test_reset();

    ic_stable_io_t *io = ic_stable_io_create();
    cr_assert_not_null(io);

    uint8_t payload[16] = {0};
    cr_expect_eq(ic_stable_io_write(io, payload, sizeof(payload)),
                 IC_STORAGE_OK);

    cr_expect_eq(ic_stable_io_seek(io, 0, IC_STABLE_SEEK_END), IC_STORAGE_OK);

    uint8_t readback = 0xAA;
    int64_t read_len = ic_stable_io_read(io, &readback, sizeof(readback));
    cr_expect_eq(read_len, 0);

    ic_stable_io_free(io);
}

Test(ic_stable_io, seek_rejects_negative_offset) {
    ic_storage_test_reset();

    ic_stable_io_t *io = ic_stable_io_create();
    cr_assert_not_null(io);

    cr_expect_eq(ic_stable_io_seek(io, -1, IC_STABLE_SEEK_SET),
                 IC_STORAGE_ERR_OUT_OF_BOUNDS);

    ic_stable_io_free(io);
}
