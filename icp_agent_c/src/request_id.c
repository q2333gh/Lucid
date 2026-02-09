#include "icp_agent/request_id.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <idl/leb128.h>
#include <openssl/sha.h>

typedef struct {
    uint8_t key_hash[SHA256_DIGEST_LENGTH];
    uint8_t value_hash[SHA256_DIGEST_LENGTH];
} request_id_field_hash_t;

static void sha256_bytes(const uint8_t *data, size_t len, uint8_t out[32]) {
    SHA256(data, len, out);
}

static const char *request_type_string(ic_envelope_type_t type) {
    switch (type) {
    case IC_ENVELOPE_CALL:
        return "call";
    case IC_ENVELOPE_QUERY:
        return "query";
    default:
        return NULL;
    }
}

static int compare_field_hashes(const void *lhs, const void *rhs) {
    const request_id_field_hash_t *a = (const request_id_field_hash_t *)lhs;
    const request_id_field_hash_t *b = (const request_id_field_hash_t *)rhs;
    return memcmp(a->key_hash, b->key_hash, SHA256_DIGEST_LENGTH);
}

static size_t add_field_hash(request_id_field_hash_t *fields,
                             size_t                   count,
                             const char              *key,
                             const uint8_t           *value,
                             size_t                   value_len) {
    sha256_bytes((const uint8_t *)key, strlen(key), fields[count].key_hash);
    sha256_bytes(value, value_len, fields[count].value_hash);
    return count + 1;
}

ic_agent_error_code_t
ic_compute_request_id(const ic_envelope_content_t *content,
                      ic_request_id_t             *out_id) {
    request_id_field_hash_t fields[7];
    size_t                  field_count = 0;
    uint8_t                 expiry_leb128[16];
    size_t                  expiry_len = 0;
    const char             *request_type = NULL;
    uint8_t                 concat[(7 * 2) * SHA256_DIGEST_LENGTH];
    size_t                  concat_len = 0;

    if (content == NULL || out_id == NULL) {
        return IC_AGENT_ERR;
    }
    if (content->canister_id == NULL || content->canister_id_len == 0 ||
        content->method_name == NULL || content->arg == NULL) {
        return IC_AGENT_ERR;
    }
    if (content->has_sender &&
        (content->sender == NULL || content->sender_len == 0)) {
        return IC_AGENT_ERR;
    }

    request_type = request_type_string(content->type);
    if (request_type == NULL) {
        return IC_AGENT_ERR;
    }

    field_count =
        add_field_hash(fields, field_count, "request_type",
                       (const uint8_t *)request_type, strlen(request_type));
    field_count =
        add_field_hash(fields, field_count, "canister_id", content->canister_id,
                       content->canister_id_len);
    field_count = add_field_hash(fields, field_count, "method_name",
                                 (const uint8_t *)content->method_name,
                                 strlen(content->method_name));
    field_count = add_field_hash(fields, field_count, "arg", content->arg,
                                 content->arg_len);

    if (content->has_sender) {
        field_count = add_field_hash(fields, field_count, "sender",
                                     content->sender, content->sender_len);
    }

    if (content->has_ingress_expiry) {
        if (idl_uleb128_encode(content->ingress_expiry, expiry_leb128,
                               sizeof(expiry_leb128),
                               &expiry_len) != IDL_STATUS_OK) {
            return IC_AGENT_ERR;
        }
        field_count = add_field_hash(fields, field_count, "ingress_expiry",
                                     expiry_leb128, expiry_len);
    }

    qsort(fields, field_count, sizeof(fields[0]), compare_field_hashes);

    for (size_t i = 0; i < field_count; i++) {
        memcpy(concat + concat_len, fields[i].key_hash, SHA256_DIGEST_LENGTH);
        concat_len += SHA256_DIGEST_LENGTH;
        memcpy(concat + concat_len, fields[i].value_hash, SHA256_DIGEST_LENGTH);
        concat_len += SHA256_DIGEST_LENGTH;
    }

    sha256_bytes(concat, concat_len, out_id->hash);
    return IC_AGENT_OK;
}
