#include "icp_agent/envelope.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} cbor_buf_t;

static int cbor_reserve(cbor_buf_t *buf, size_t add_len) {
    size_t   needed = 0;
    size_t   new_cap = 0;
    uint8_t *new_data = NULL;
    if (buf == NULL) {
        return 0;
    }
    needed = buf->len + add_len;
    if (needed <= buf->cap) {
        return 1;
    }
    new_cap = (buf->cap == 0) ? 128 : buf->cap;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    new_data = (uint8_t *)realloc(buf->data, new_cap);
    if (new_data == NULL) {
        return 0;
    }
    buf->data = new_data;
    buf->cap = new_cap;
    return 1;
}

static int cbor_push_byte(cbor_buf_t *buf, uint8_t byte) {
    if (!cbor_reserve(buf, 1)) {
        return 0;
    }
    buf->data[buf->len++] = byte;
    return 1;
}

static int cbor_push_bytes(cbor_buf_t *buf, const uint8_t *data, size_t len) {
    if (!cbor_reserve(buf, len)) {
        return 0;
    }
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    return 1;
}

static int cbor_write_major(cbor_buf_t *buf, uint8_t major, uint64_t value) {
    if (value <= 23) {
        return cbor_push_byte(buf, (uint8_t)((major << 5) | (uint8_t)value));
    }
    if (value <= 0xFF) {
        return cbor_push_byte(buf, (uint8_t)((major << 5) | 24)) &&
               cbor_push_byte(buf, (uint8_t)value);
    }
    if (value <= 0xFFFF) {
        return cbor_push_byte(buf, (uint8_t)((major << 5) | 25)) &&
               cbor_push_byte(buf, (uint8_t)((value >> 8) & 0xFF)) &&
               cbor_push_byte(buf, (uint8_t)(value & 0xFF));
    }
    if (value <= 0xFFFFFFFFULL) {
        return cbor_push_byte(buf, (uint8_t)((major << 5) | 26)) &&
               cbor_push_byte(buf, (uint8_t)((value >> 24) & 0xFF)) &&
               cbor_push_byte(buf, (uint8_t)((value >> 16) & 0xFF)) &&
               cbor_push_byte(buf, (uint8_t)((value >> 8) & 0xFF)) &&
               cbor_push_byte(buf, (uint8_t)(value & 0xFF));
    }
    return cbor_push_byte(buf, (uint8_t)((major << 5) | 27)) &&
           cbor_push_byte(buf, (uint8_t)((value >> 56) & 0xFF)) &&
           cbor_push_byte(buf, (uint8_t)((value >> 48) & 0xFF)) &&
           cbor_push_byte(buf, (uint8_t)((value >> 40) & 0xFF)) &&
           cbor_push_byte(buf, (uint8_t)((value >> 32) & 0xFF)) &&
           cbor_push_byte(buf, (uint8_t)((value >> 24) & 0xFF)) &&
           cbor_push_byte(buf, (uint8_t)((value >> 16) & 0xFF)) &&
           cbor_push_byte(buf, (uint8_t)((value >> 8) & 0xFF)) &&
           cbor_push_byte(buf, (uint8_t)(value & 0xFF));
}

static int cbor_write_text(cbor_buf_t *buf, const char *text) {
    size_t len = 0;
    if (text == NULL) {
        return 0;
    }
    len = strlen(text);
    return cbor_write_major(buf, 3, (uint64_t)len) &&
           cbor_push_bytes(buf, (const uint8_t *)text, len);
}

static int cbor_write_bytes(cbor_buf_t *buf, const uint8_t *data, size_t len) {
    return cbor_write_major(buf, 2, (uint64_t)len) &&
           cbor_push_bytes(buf, data, len);
}

static int cbor_write_null(cbor_buf_t *buf) {
    return cbor_push_byte(buf, 0xF6);
}

static int cbor_write_array_len(cbor_buf_t *buf, size_t len) {
    return cbor_write_major(buf, 4, (uint64_t)len);
}

static const char *request_type_to_text(ic_envelope_type_t type) {
    switch (type) {
    case IC_ENVELOPE_CALL:
        return "call";
    case IC_ENVELOPE_QUERY:
        return "query";
    case IC_ENVELOPE_READ_STATE:
        return "read_state";
    default:
        return NULL;
    }
}

static int cbor_write_content(cbor_buf_t                  *buf,
                              const ic_envelope_content_t *content) {
    const char *request_type = request_type_to_text(content->type);
    size_t      map_len = 0;

    if (request_type == NULL) {
        return 0;
    }
    if (content->type == IC_ENVELOPE_READ_STATE) {
        map_len = 4;
    } else {
        map_len = 6;
    }

    if (!cbor_write_major(buf, 5, (uint64_t)map_len)) {
        return 0;
    }
    if (!cbor_write_text(buf, "request_type") ||
        !cbor_write_text(buf, request_type)) {
        return 0;
    }
    if (!content->has_sender || content->sender == NULL ||
        content->sender_len == 0 || !content->has_ingress_expiry) {
        return 0;
    }
    if (content->type == IC_ENVELOPE_READ_STATE) {
        if (!cbor_write_text(buf, "sender") ||
            !cbor_write_bytes(buf, content->sender, content->sender_len) ||
            !cbor_write_text(buf, "ingress_expiry") ||
            !cbor_write_major(buf, 0, content->ingress_expiry) ||
            !cbor_write_text(buf, "paths") || !cbor_write_array_len(buf, 1) ||
            !cbor_write_array_len(buf, 2) ||
            !cbor_write_text(buf, "request_status") ||
            !cbor_write_bytes(buf, content->arg, content->arg_len)) {
            return 0;
        }
    } else {
        if (!cbor_write_text(buf, "canister_id") ||
            !cbor_write_bytes(buf, content->canister_id,
                              content->canister_id_len) ||
            !cbor_write_text(buf, "method_name") ||
            !cbor_write_text(buf, content->method_name) ||
            !cbor_write_text(buf, "arg") ||
            !cbor_write_bytes(buf, content->arg, content->arg_len) ||
            !cbor_write_text(buf, "sender") ||
            !cbor_write_bytes(buf, content->sender, content->sender_len) ||
            !cbor_write_text(buf, "ingress_expiry") ||
            !cbor_write_major(buf, 0, content->ingress_expiry)) {
            return 0;
        }
    }
    return 1;
}

ic_agent_error_code_t
ic_encode_envelope_cbor(const ic_envelope_content_t *content,
                        const uint8_t               *sender_pubkey,
                        size_t                       sender_pubkey_len,
                        const uint8_t               *sender_sig,
                        size_t                       sender_sig_len,
                        uint8_t                    **out_cbor,
                        size_t                      *out_cbor_len) {
    cbor_buf_t buf = {0};

    if (content == NULL || out_cbor == NULL || out_cbor_len == NULL ||
        content->arg == NULL) {
        return IC_AGENT_ERR;
    }
    if (content->type != IC_ENVELOPE_READ_STATE &&
        (content->canister_id == NULL || content->method_name == NULL)) {
        return IC_AGENT_ERR;
    }
    if (content->has_sender &&
        (content->sender == NULL || content->sender_len == 0)) {
        return IC_AGENT_ERR;
    }

    if (!cbor_push_byte(&buf, 0xD9) || !cbor_push_byte(&buf, 0xD9) ||
        !cbor_push_byte(&buf, 0xF7)) {
        free(buf.data);
        return IC_AGENT_ERR;
    }

    if (!cbor_write_major(&buf, 5, 4)) {
        free(buf.data);
        return IC_AGENT_ERR;
    }
    if (!cbor_write_text(&buf, "content") ||
        !cbor_write_content(&buf, content)) {
        free(buf.data);
        return IC_AGENT_ERR;
    }
    if (!cbor_write_text(&buf, "sender_pubkey")) {
        free(buf.data);
        return IC_AGENT_ERR;
    }
    if (sender_pubkey != NULL && sender_pubkey_len > 0) {
        if (!cbor_write_bytes(&buf, sender_pubkey, sender_pubkey_len)) {
            free(buf.data);
            return IC_AGENT_ERR;
        }
    } else if (!cbor_write_null(&buf)) {
        free(buf.data);
        return IC_AGENT_ERR;
    }

    if (!cbor_write_text(&buf, "sender_sig")) {
        free(buf.data);
        return IC_AGENT_ERR;
    }
    if (sender_sig != NULL && sender_sig_len > 0) {
        if (!cbor_write_bytes(&buf, sender_sig, sender_sig_len)) {
            free(buf.data);
            return IC_AGENT_ERR;
        }
    } else if (!cbor_write_null(&buf)) {
        free(buf.data);
        return IC_AGENT_ERR;
    }

    if (!cbor_write_text(&buf, "sender_delegation") || !cbor_write_null(&buf)) {
        free(buf.data);
        return IC_AGENT_ERR;
    }

    *out_cbor = buf.data;
    *out_cbor_len = buf.len;
    return IC_AGENT_OK;
}

void ic_envelope_cbor_free(uint8_t *cbor_bytes) { free(cbor_bytes); }
