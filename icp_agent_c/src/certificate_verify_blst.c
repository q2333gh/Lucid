#include "icp_agent/certificate.h"

#include <string.h>

typedef struct {
    const uint8_t *buf;
    size_t         len;
    size_t         off;
} cbor_reader_t;

static int cbor_read_u64(cbor_reader_t *r, uint8_t ai, uint64_t *out) {
    if (ai <= 23) {
        *out = ai;
        return 1;
    }
    if (ai == 24) {
        if (r->off + 1 > r->len)
            return 0;
        *out = r->buf[r->off++];
        return 1;
    }
    if (ai == 25) {
        if (r->off + 2 > r->len)
            return 0;
        *out = ((uint64_t)r->buf[r->off] << 8) | r->buf[r->off + 1];
        r->off += 2;
        return 1;
    }
    if (ai == 26) {
        if (r->off + 4 > r->len)
            return 0;
        *out = ((uint64_t)r->buf[r->off] << 24) |
               ((uint64_t)r->buf[r->off + 1] << 16) |
               ((uint64_t)r->buf[r->off + 2] << 8) | r->buf[r->off + 3];
        r->off += 4;
        return 1;
    }
    return 0;
}

static int cbor_read_header(cbor_reader_t *r, uint8_t *major, uint64_t *value) {
    uint8_t ib = 0;
    if (r->off >= r->len)
        return 0;
    ib = r->buf[r->off++];
    *major = (uint8_t)(ib >> 5);
    return cbor_read_u64(r, (uint8_t)(ib & 0x1F), value);
}

static int cbor_read_text(cbor_reader_t *r, const uint8_t **ptr, size_t *len) {
    uint8_t  major = 0;
    uint64_t n = 0;
    if (!cbor_read_header(r, &major, &n) || major != 3)
        return 0;
    if (r->off + n > r->len)
        return 0;
    *ptr = r->buf + r->off;
    *len = (size_t)n;
    r->off += (size_t)n;
    return 1;
}

static int cbor_skip_value(cbor_reader_t *r);

static int cbor_skip_map(cbor_reader_t *r, size_t pairs) {
    for (size_t i = 0; i < pairs; i++) {
        if (!cbor_skip_value(r) || !cbor_skip_value(r))
            return 0;
    }
    return 1;
}

static int cbor_skip_value(cbor_reader_t *r) {
    uint8_t  major = 0;
    uint64_t n = 0;
    if (!cbor_read_header(r, &major, &n))
        return 0;
    if (major == 0 || major == 1)
        return 1;
    if (major == 2 || major == 3) {
        if (r->off + n > r->len)
            return 0;
        r->off += (size_t)n;
        return 1;
    }
    if (major == 5)
        return cbor_skip_map(r, (size_t)n);
    if (major == 7 && n == 22)
        return 1;
    return 0;
}

static int cbor_read_bytes(cbor_reader_t *r, const uint8_t **ptr, size_t *len) {
    uint8_t  major = 0;
    uint64_t n = 0;
    if (!cbor_read_header(r, &major, &n) || major != 2)
        return 0;
    if (r->off + n > r->len)
        return 0;
    *ptr = r->buf + r->off;
    *len = (size_t)n;
    r->off += (size_t)n;
    return 1;
}

static int key_equals(const uint8_t *k, size_t klen, const char *lit) {
    size_t n = strlen(lit);
    return klen == n && memcmp(k, lit, n) == 0;
}

static int has_certificate_field(const uint8_t *cert_cbor,
                                 size_t         cert_cbor_len) {
    cbor_reader_t r = {cert_cbor, cert_cbor_len, 0};
    uint8_t       major = 0;
    uint64_t      pairs = 0;

    if (r.len >= 3 && r.buf[0] == 0xD9 && r.buf[1] == 0xD9 &&
        r.buf[2] == 0xF7) {
        r.off = 3;
    }
    if (!cbor_read_header(&r, &major, &pairs) || major != 5)
        return 0;

    for (size_t i = 0; i < (size_t)pairs; i++) {
        const uint8_t *k = NULL;
        size_t         klen = 0;
        if (!cbor_read_text(&r, &k, &klen))
            return 0;
        if (key_equals(k, klen, "certificate")) {
            const uint8_t *blob = NULL;
            size_t         blob_len = 0;
            if (!cbor_read_bytes(&r, &blob, &blob_len))
                return 0;
            return blob_len > 0;
        }
        if (!cbor_skip_value(&r))
            return 0;
    }
    return 0;
}

bool ic_certificate_verify_available(void) {
#if defined(ICP_AGENT_ENABLE_BLST)
    return true;
#else
    return false;
#endif
}

ic_agent_error_code_t
ic_verify_certificate(const uint8_t *cert_cbor,
                      size_t         cert_cbor_len,
                      const uint8_t *root_key,
                      size_t         root_key_len,
                      const uint8_t *effective_canister_id,
                      size_t         effective_canister_id_len) {
    if (cert_cbor == NULL || cert_cbor_len == 0 || root_key == NULL ||
        root_key_len == 0 || effective_canister_id == NULL ||
        effective_canister_id_len == 0) {
        return IC_AGENT_ERR;
    }
    if (!has_certificate_field(cert_cbor, cert_cbor_len)) {
        return IC_AGENT_ERR_INVALID_CBOR;
    }

#if defined(ICP_AGENT_ENABLE_BLST)
    return IC_AGENT_ERR_VERIFY_FAILED;
#else
    (void)root_key;
    (void)root_key_len;
    (void)effective_canister_id;
    (void)effective_canister_id_len;
    return IC_AGENT_ERR_UNSUPPORTED;
#endif
}
