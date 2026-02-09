#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/x509.h>

#include "icp_agent/identity.h"

static int verify_ed25519_der_pubkey(const uint8_t *der_pubkey,
                                     size_t         der_pubkey_len,
                                     const uint8_t *message,
                                     size_t         message_len,
                                     const uint8_t *signature,
                                     size_t         signature_len) {
    int            ok = 0;
    EVP_PKEY      *pkey = NULL;
    EVP_MD_CTX    *ctx = NULL;
    const uint8_t *p = der_pubkey;

    pkey = d2i_PUBKEY(NULL, &p, (long)der_pubkey_len);
    if (pkey == NULL) {
        goto cleanup;
    }

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        goto cleanup;
    }

    if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey) != 1) {
        goto cleanup;
    }

    ok = EVP_DigestVerify(ctx, signature, signature_len, message,
                          message_len) == 1;

cleanup:
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
}

int main(void) {
    ic_identity_t   identity;
    ic_request_id_t request_id;
    uint8_t        *sig = NULL;
    size_t          sig_len = 0;
    uint8_t        *pubkey = NULL;
    size_t          pubkey_len = 0;
    uint8_t         signable[43];

    memset(&identity, 0, sizeof(identity));
    memset(&request_id, 0, sizeof(request_id));
    for (size_t i = 0; i < sizeof(request_id.hash); i++) {
        request_id.hash[i] = (uint8_t)i;
    }

    assert(ic_identity_anonymous_init(&identity) == IC_AGENT_OK);
    assert(ic_identity_sign_request_id(&identity, &request_id, &sig,
                                       &sig_len) == IC_AGENT_OK);
    assert(sig == NULL);
    assert(sig_len == 0);
    assert(ic_identity_public_key_der(&identity, &pubkey, &pubkey_len) ==
           IC_AGENT_OK);
    assert(pubkey == NULL);
    assert(pubkey_len == 0);
    ic_identity_destroy(&identity);

    assert(ic_identity_ed25519_generate(&identity) == IC_AGENT_OK);
    assert(ic_identity_sign_request_id(&identity, &request_id, &sig,
                                       &sig_len) == IC_AGENT_OK);
    assert(sig != NULL);
    assert(sig_len == 64);
    assert(ic_identity_public_key_der(&identity, &pubkey, &pubkey_len) ==
           IC_AGENT_OK);
    assert(pubkey != NULL);
    assert(pubkey_len > 0);

    memcpy(signable, "\x0Aic-request", 11);
    memcpy(signable + 11, request_id.hash, 32);
    assert(verify_ed25519_der_pubkey(pubkey, pubkey_len, signable,
                                     sizeof(signable), sig, sig_len) == 1);
    assert(verify_ed25519_der_pubkey(pubkey, pubkey_len, request_id.hash,
                                     sizeof(request_id.hash), sig,
                                     sig_len) == 0);

    ic_identity_bytes_free(sig);
    ic_identity_bytes_free(pubkey);
    ic_identity_destroy(&identity);
    return 0;
}
