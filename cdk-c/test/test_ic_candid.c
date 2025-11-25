#include <criterion/criterion.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ic_buffer.h"
#include "ic_candid.h"
#include "ic_principal.h"

// Tiny helper to keep buffer initialization consistent across tests.
static ic_buffer_t make_buffer(void) {
    ic_buffer_t buf;
    cr_expect_eq(ic_buffer_init(&buf), IC_OK);
    return buf;
}

// Unsigned LEB128 should round-trip a representative range, including max
// values.
Test(ic_candid, leb128_round_trip_values) {
    const uint64_t values[] = {0ULL,   1ULL,      127ULL,
                               128ULL, 624485ULL, UINT64_MAX};

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        ic_buffer_t buf = make_buffer();
        cr_expect_eq(candid_write_leb128(&buf, values[i]), IC_OK);

        size_t offset = 0;
        uint64_t decoded = 0;
        const uint8_t* raw = ic_buffer_data(&buf);
        cr_expect_eq(
            candid_read_leb128(raw, ic_buffer_size(&buf), &offset, &decoded),
            IC_OK);
        cr_expect_eq(decoded, values[i]);
        cr_expect_eq(offset, ic_buffer_size(&buf));

        ic_buffer_free(&buf);
    }

    size_t offset = 0;
    uint64_t decoded = 0;
    cr_expect_eq(candid_read_leb128(NULL, 0, &offset, &decoded),
                 IC_ERR_INVALID_ARG);
    ic_buffer_t invalid = make_buffer();
    cr_expect_eq(candid_write_leb128(NULL, 42), IC_ERR_INVALID_ARG);
    ic_buffer_free(&invalid);
}

// Signed LEB128 needs to decode negatives and detect truncated payloads.
Test(ic_candid, sleb128_decodes_negative_numbers_and_errors_on_truncation) {
    const uint8_t encoded_negative[] = {0xC0, 0xBB, 0x78};  // -123456
    size_t offset = 0;
    int64_t decoded = 0;
    cr_expect_eq(candid_read_sleb128(encoded_negative, sizeof(encoded_negative),
                                     &offset, &decoded),
                 IC_OK);
    cr_expect_eq(decoded, -123456);
    cr_expect_eq(offset, sizeof(encoded_negative));

    const uint8_t truncated[] = {0x80};
    offset = 0;
    cr_expect_eq(
        candid_read_sleb128(truncated, sizeof(truncated), &offset, &decoded),
        IC_ERR_INVALID_ARG);
    cr_expect_eq(candid_read_sleb128(NULL, 0, &offset, &decoded),
                 IC_ERR_INVALID_ARG);
}

// Text serialization must capture both payload and type tag round-tripping.
Test(ic_candid, serialize_and_deserialize_text) {
    ic_buffer_t buf = make_buffer();
    const char* message = "hello lucid";
    cr_expect_eq(candid_serialize_text(&buf, message), IC_OK);

    size_t offset = 0;
    char* decoded = NULL;
    size_t decoded_len = 0;
    cr_expect_eq(
        candid_deserialize_text(ic_buffer_data(&buf), ic_buffer_size(&buf),
                                &offset, &decoded, &decoded_len),
        IC_OK);
    cr_expect_str_eq(decoded, message);
    cr_expect_eq(decoded_len, strlen(message));
    cr_expect_eq(offset, ic_buffer_size(&buf));
    free(decoded);

    ic_buffer_free(&buf);

    // Wrong type tag must fail
    const uint8_t wrong_tag[] = {CANDID_TYPE_NAT};
    offset = 0;
    decoded = NULL;
    cr_expect_eq(candid_deserialize_text(wrong_tag, sizeof(wrong_tag), &offset,
                                         &decoded, &decoded_len),
                 IC_ERR_INVALID_ARG);
}

// Integer helpers should cover both nat (unsigned) and int (signed) paths.
Test(ic_candid, serialize_and_deserialize_nat_and_int) {
    ic_buffer_t buf = make_buffer();
    const uint64_t nat_value = 9007199254740991ULL;
    cr_expect_eq(candid_serialize_nat(&buf, nat_value), IC_OK);

    size_t offset = 0;
    uint64_t decoded_nat = 0;
    cr_expect_eq(
        candid_deserialize_nat(ic_buffer_data(&buf), ic_buffer_size(&buf),
                               &offset, &decoded_nat),
        IC_OK);
    cr_expect_eq(decoded_nat, nat_value);
    cr_expect_eq(offset, ic_buffer_size(&buf));
    ic_buffer_free(&buf);

    buf = make_buffer();
    const int64_t int_value = -987654321;
    cr_expect_eq(candid_serialize_int(&buf, int_value), IC_OK);
    offset = 0;
    int64_t decoded_int = 0;
    cr_expect_eq(
        candid_deserialize_int(ic_buffer_data(&buf), ic_buffer_size(&buf),
                               &offset, &decoded_int),
        IC_OK);
    cr_expect_eq(decoded_int, int_value);
    cr_expect_eq(offset, ic_buffer_size(&buf));
    ic_buffer_free(&buf);
}

// Binary blobs and principals exercise the more complex structured encoders.
Test(ic_candid, serialize_and_deserialize_blob_and_principal) {
    ic_buffer_t buf = make_buffer();
    const uint8_t blob[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
    cr_expect_eq(candid_serialize_blob(&buf, blob, sizeof(blob)), IC_OK);

    size_t offset = 0;
    uint8_t* decoded_blob = NULL;
    size_t decoded_len = 0;
    cr_expect_eq(
        candid_deserialize_blob(ic_buffer_data(&buf), ic_buffer_size(&buf),
                                &offset, &decoded_blob, &decoded_len),
        IC_OK);
    cr_expect_eq(decoded_len, sizeof(blob));
    cr_expect_arr_eq(decoded_blob, blob, sizeof(blob));
    cr_expect_eq(offset, ic_buffer_size(&buf));
    free(decoded_blob);
    ic_buffer_free(&buf);

    ic_principal_t principal = {0};
    const uint8_t principal_bytes[] = {0x01, 0x23, 0x45};
    cr_expect_eq(ic_principal_from_bytes(&principal, principal_bytes,
                                         sizeof(principal_bytes)),
                 IC_OK);

    buf = make_buffer();
    cr_expect_eq(candid_serialize_principal(&buf, &principal), IC_OK);
    offset = 0;
    ic_principal_t decoded_principal = {0};
    cr_expect_eq(
        candid_deserialize_principal(ic_buffer_data(&buf), ic_buffer_size(&buf),
                                     &offset, &decoded_principal),
        IC_OK);
    cr_expect(ic_principal_equal(&principal, &decoded_principal));
    ic_buffer_free(&buf);

    ic_principal_t invalid = {0};
    buf = make_buffer();
    cr_expect_eq(candid_serialize_principal(&buf, &invalid),
                 IC_ERR_INVALID_ARG);
    ic_buffer_free(&buf);
}

// Quick sanity check: magic header detection rejects short/invalid payloads.
Test(ic_candid, candid_magic_header_detection) {
    const uint8_t valid[] = {'D', 'I', 'D', 'L', 0x00};
    const uint8_t invalid[] = {'D', 'I', 'D'};

    cr_expect(candid_check_magic(valid, sizeof(valid)));
    cr_expect_not(candid_check_magic(invalid, sizeof(invalid)));
    cr_expect_not(candid_check_magic(NULL, 0));
}
