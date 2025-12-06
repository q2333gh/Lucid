#include <criterion/criterion.h>
#include <stdint.h>
#include <string.h>

#include "ic_principal.h"

// Happy-path: bytes → principals → equality comparison stays stable.
Test(ic_principal, from_bytes_and_equality) {
    ic_principal_t left = {0};
    ic_principal_t right = {0};
    const uint8_t  raw[] = {0xAB, 0xCD, 0xEF, 0x01};

    cr_expect_eq(ic_principal_from_bytes(&left, raw, sizeof(raw)), IC_OK);
    cr_expect_eq(ic_principal_from_bytes(&right, raw, sizeof(raw)), IC_OK);
    cr_expect_eq(left.len, sizeof(raw));
    cr_expect_eq(memcmp(left.bytes, raw, sizeof(raw)), 0);
    cr_expect(ic_principal_equal(&left, &right));

    right.bytes[0] ^= 0xFF;
    cr_expect_not(ic_principal_equal(&left, &right));
}

// Constructors must reject null pointers, zero-length data, and oversized
// blobs.
Test(ic_principal, from_bytes_validates_input) {
    ic_principal_t principal = {0};
    uint8_t        oversized[IC_PRINCIPAL_MAX_LEN + 1] = {0};

    cr_expect_eq(ic_principal_from_bytes(&principal, NULL, 1),
                 IC_ERR_INVALID_ARG);
    cr_expect_eq(ic_principal_from_bytes(NULL, oversized, 1),
                 IC_ERR_INVALID_ARG);
    cr_expect_eq(ic_principal_from_bytes(&principal, oversized, 0),
                 IC_ERR_INVALID_ARG);
    cr_expect_eq(
        ic_principal_from_bytes(&principal, oversized, sizeof(oversized)),
        IC_ERR_INVALID_ARG);
}

// Encoding should yield the stable, expected base32 output for known fixtures.
Test(ic_principal, to_text_encodes_expected_base32_prefix) {
    ic_principal_t principal = {0};
    const uint8_t  raw[] = {0xAB, 0xCD, 0x01};
    cr_expect_eq(ic_principal_from_bytes(&principal, raw, sizeof(raw)), IC_OK);

    char text[64] = {0};
    int  written = ic_principal_to_text(&principal, text, sizeof(text));
    cr_expect_gt(written, 0);
    cr_expect_str_eq(text, "em77e-bvlzu-aq");
}

// Text conversion must fail fast when buffers are too small or inputs invalid.
Test(ic_principal, to_text_checks_buffer_and_input) {
    ic_principal_t principal = {0};
    const uint8_t  raw[] = {0x01};
    cr_expect_eq(ic_principal_from_bytes(&principal, raw, sizeof(raw)), IC_OK);

    char tiny[4] = {0};
    cr_expect_eq(ic_principal_to_text(&principal, tiny, sizeof(tiny)), -1);

    ic_principal_t invalid = {0};
    char           buf[8] = {0};
    cr_expect_eq(ic_principal_to_text(&invalid, buf, sizeof(buf)), -1);
}
