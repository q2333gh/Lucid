#include "icp_agent/agent.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ic_principal.h"
#include "icp_agent/envelope.h"
#include "icp_agent/request_id.h"
#include "icp_agent/transport.h"
#include "icp_agent/types.h"

const char *ic_agent_version(void) { return "0.1.0-dev"; }

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

static int key_equals(const uint8_t *k, size_t klen, const char *lit) {
    size_t n = strlen(lit);
    return klen == n && memcmp(k, lit, n) == 0;
}

static int build_canister_api_path(const uint8_t *canister_id,
                                   size_t         canister_id_len,
                                   const char    *suffix,
                                   char          *out,
                                   size_t         out_len) {
    ic_principal_t principal;
    char           principal_text[IC_PRINCIPAL_MAX_LEN * 2 + 1];
    int            text_len = 0;
    int            n = 0;

    if (canister_id == NULL || canister_id_len == 0 || suffix == NULL ||
        out == NULL || out_len == 0) {
        return 0;
    }
    if (ic_principal_from_bytes(&principal, canister_id, canister_id_len) !=
        IC_OK) {
        return 0;
    }
    text_len = ic_principal_to_text(&principal, principal_text,
                                    sizeof(principal_text));
    if (text_len <= 0)
        return 0;

    n = snprintf(out, out_len, "/api/v3/canister/%s/%s", principal_text,
                 suffix);
    return n > 0 && (size_t)n < out_len;
}

static int parse_query_replied_arg(const uint8_t *buf,
                                   size_t         len,
                                   uint8_t      **out_arg,
                                   size_t        *out_arg_len) {
    cbor_reader_t  r = {buf, len, 0};
    uint8_t        major = 0;
    uint64_t       pairs = 0;
    const uint8_t *key = NULL;
    size_t         key_len = 0;
    bool           is_replied = false;

    if (r.len >= 3 && r.buf[0] == 0xD9 && r.buf[1] == 0xD9 &&
        r.buf[2] == 0xF7) {
        r.off = 3;
    }
    if (!cbor_read_header(&r, &major, &pairs) || major != 5)
        return 0;
    for (size_t i = 0; i < (size_t)pairs; i++) {
        if (!cbor_read_text(&r, &key, &key_len))
            return 0;
        if (key_equals(key, key_len, "status")) {
            const uint8_t *v = NULL;
            size_t         vlen = 0;
            if (!cbor_read_text(&r, &v, &vlen))
                return 0;
            is_replied = (vlen == 7 && memcmp(v, "replied", 7) == 0);
        } else if (key_equals(key, key_len, "reply")) {
            uint8_t  m2 = 0;
            uint64_t p2 = 0;
            if (!cbor_read_header(&r, &m2, &p2) || m2 != 5)
                return 0;
            for (size_t j = 0; j < (size_t)p2; j++) {
                const uint8_t *rk = NULL;
                size_t         rklen = 0;
                if (!cbor_read_text(&r, &rk, &rklen))
                    return 0;
                if (key_equals(rk, rklen, "arg")) {
                    const uint8_t *arg = NULL;
                    size_t         arg_len = 0;
                    uint8_t       *copy = NULL;
                    if (!cbor_read_bytes(&r, &arg, &arg_len))
                        return 0;
                    copy = (uint8_t *)malloc(arg_len);
                    if (copy == NULL)
                        return 0;
                    memcpy(copy, arg, arg_len);
                    *out_arg = copy;
                    *out_arg_len = arg_len;
                } else if (!cbor_skip_value(&r)) {
                    return 0;
                }
            }
        } else if (!cbor_skip_value(&r)) {
            return 0;
        }
    }
    return is_replied && *out_arg != NULL;
}

ic_agent_error_code_t ic_agent_init(ic_agent_t *agent,
                                    const char *replica_url) {
    size_t n = 0;
    if (agent == NULL || replica_url == NULL)
        return IC_AGENT_ERR;
    n = strlen(replica_url);
    agent->replica_url = (char *)malloc(n + 1);
    if (agent->replica_url == NULL)
        return IC_AGENT_ERR;
    memcpy(agent->replica_url, replica_url, n + 1);
    return IC_AGENT_OK;
}

void ic_agent_destroy(ic_agent_t *agent) {
    if (agent == NULL)
        return;
    free(agent->replica_url);
    agent->replica_url = NULL;
}

static ic_agent_error_code_t post_envelope(ic_agent_t                  *agent,
                                           const ic_envelope_content_t *content,
                                           const char         *path_suffix,
                                           ic_http_response_t *out_response) {
    uint8_t              *envelope = NULL;
    size_t                envelope_len = 0;
    char                 *url = NULL;
    size_t                url_len = 0;
    ic_agent_error_code_t rc = IC_AGENT_ERR;

    if (agent == NULL || agent->replica_url == NULL || content == NULL ||
        path_suffix == NULL || out_response == NULL) {
        return IC_AGENT_ERR;
    }
    if (ic_encode_envelope_cbor(content, NULL, 0, NULL, 0, &envelope,
                                &envelope_len) != IC_AGENT_OK) {
        return IC_AGENT_ERR;
    }

    url_len = strlen(agent->replica_url) + strlen(path_suffix);
    url = (char *)malloc(url_len + 1);
    if (url == NULL) {
        ic_envelope_cbor_free(envelope);
        return IC_AGENT_ERR;
    }
    strcpy(url, agent->replica_url);
    strcat(url, path_suffix);
    rc = ic_http_post_binary(url, envelope, envelope_len, out_response);
    free(url);
    ic_envelope_cbor_free(envelope);
    return rc;
}

ic_agent_error_code_t ic_agent_query(ic_agent_t    *agent,
                                     const uint8_t *canister_id,
                                     size_t         canister_id_len,
                                     const char    *method_name,
                                     const uint8_t *arg,
                                     size_t         arg_len,
                                     uint8_t      **out_reply,
                                     size_t        *out_reply_len) {
    ic_envelope_content_t content;
    ic_http_response_t    response = {0};
    ic_agent_error_code_t rc = IC_AGENT_ERR;
    char                  path[128];

    if (agent == NULL || agent->replica_url == NULL || canister_id == NULL ||
        canister_id_len == 0 || method_name == NULL || arg == NULL ||
        out_reply == NULL || out_reply_len == NULL) {
        return IC_AGENT_ERR;
    }
    *out_reply = NULL;
    *out_reply_len = 0;

    content.type = IC_ENVELOPE_QUERY;
    content.canister_id = canister_id;
    content.canister_id_len = canister_id_len;
    content.method_name = method_name;
    content.arg = arg;
    content.arg_len = arg_len;
    content.sender = NULL;
    content.sender_len = 0;
    content.has_sender = false;
    content.ingress_expiry = 0;
    content.has_ingress_expiry = false;

    if (!build_canister_api_path(canister_id, canister_id_len, "query", path,
                                 sizeof(path))) {
        return IC_AGENT_ERR;
    }
    rc = post_envelope(agent, &content, path, &response);
    if (rc != IC_AGENT_OK)
        return IC_AGENT_ERR;
    if (response.status_code < 200 || response.status_code >= 300) {
        ic_http_response_free(&response);
        return IC_AGENT_ERR;
    }

    rc = parse_query_replied_arg(response.body, response.body_len, out_reply,
                                 out_reply_len)
             ? IC_AGENT_OK
             : IC_AGENT_ERR;
    ic_http_response_free(&response);
    return rc;
}

ic_agent_error_code_t ic_agent_update(ic_agent_t      *agent,
                                      const uint8_t   *canister_id,
                                      size_t           canister_id_len,
                                      const char      *method_name,
                                      const uint8_t   *arg,
                                      size_t           arg_len,
                                      ic_request_id_t *out_request_id) {
    ic_envelope_content_t content;
    ic_http_response_t    response = {0};
    const uint8_t        *status_ptr = NULL;
    size_t                status_len = 0;
    bool                  status_ok = false;
    cbor_reader_t         r;
    uint8_t               major = 0;
    uint64_t              pairs = 0;
    char                  path[128];

    if (agent == NULL || out_request_id == NULL || canister_id == NULL ||
        canister_id_len == 0 || method_name == NULL || arg == NULL) {
        return IC_AGENT_ERR;
    }

    content.type = IC_ENVELOPE_CALL;
    content.canister_id = canister_id;
    content.canister_id_len = canister_id_len;
    content.method_name = method_name;
    content.arg = arg;
    content.arg_len = arg_len;
    content.sender = NULL;
    content.sender_len = 0;
    content.has_sender = false;
    content.ingress_expiry = 0;
    content.has_ingress_expiry = false;

    if (ic_compute_request_id(&content, out_request_id) != IC_AGENT_OK) {
        return IC_AGENT_ERR;
    }
    if (!build_canister_api_path(canister_id, canister_id_len, "call", path,
                                 sizeof(path))) {
        return IC_AGENT_ERR;
    }
    if (post_envelope(agent, &content, path, &response) != IC_AGENT_OK) {
        return IC_AGENT_ERR;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        ic_http_response_free(&response);
        return IC_AGENT_ERR;
    }

    r.buf = response.body;
    r.len = response.body_len;
    r.off = 0;
    if (r.len >= 3 && r.buf[0] == 0xD9 && r.buf[1] == 0xD9 &&
        r.buf[2] == 0xF7) {
        r.off = 3;
    }
    if (!cbor_read_header(&r, &major, &pairs) || major != 5) {
        ic_http_response_free(&response);
        return IC_AGENT_ERR;
    }
    for (size_t i = 0; i < (size_t)pairs; i++) {
        const uint8_t *k = NULL;
        size_t         klen = 0;
        if (!cbor_read_text(&r, &k, &klen)) {
            ic_http_response_free(&response);
            return IC_AGENT_ERR;
        }
        if (key_equals(k, klen, "status")) {
            if (!cbor_read_text(&r, &status_ptr, &status_len)) {
                ic_http_response_free(&response);
                return IC_AGENT_ERR;
            }
        } else if (!cbor_skip_value(&r)) {
            ic_http_response_free(&response);
            return IC_AGENT_ERR;
        }
    }
    if (status_ptr != NULL &&
        ((status_len == 8 && memcmp(status_ptr, "accepted", 8) == 0) ||
         (status_len == 7 && memcmp(status_ptr, "replied", 7) == 0))) {
        status_ok = true;
    }
    ic_http_response_free(&response);
    if (!status_ok) {
        return IC_AGENT_ERR;
    }
    return IC_AGENT_OK;
}

void ic_agent_bytes_free(uint8_t *bytes) { free(bytes); }
