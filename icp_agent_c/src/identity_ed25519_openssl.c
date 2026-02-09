#include "icp_agent/identity.h"

#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/x509.h>

typedef struct {
    EVP_PKEY *pkey;
} ic_ed25519_identity_impl_t;

static ic_ed25519_identity_impl_t *
as_ed25519_impl(const ic_identity_t *identity) {
    if (identity == NULL || identity->kind != IC_IDENTITY_ED25519 ||
        identity->impl == NULL) {
        return NULL;
    }
    return (ic_ed25519_identity_impl_t *)identity->impl;
}

ic_agent_error_code_t
ic_identity_ed25519_generate(ic_identity_t *out_identity) {
    EVP_PKEY_CTX               *ctx = NULL;
    EVP_PKEY                   *pkey = NULL;
    ic_ed25519_identity_impl_t *impl = NULL;

    if (out_identity == NULL) {
        return IC_AGENT_ERR;
    }

    ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if (ctx == NULL) {
        return IC_AGENT_ERR;
    }
    if (EVP_PKEY_keygen_init(ctx) != 1 || EVP_PKEY_keygen(ctx, &pkey) != 1) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return IC_AGENT_ERR;
    }
    EVP_PKEY_CTX_free(ctx);

    impl = (ic_ed25519_identity_impl_t *)malloc(sizeof(*impl));
    if (impl == NULL) {
        EVP_PKEY_free(pkey);
        return IC_AGENT_ERR;
    }
    impl->pkey = pkey;

    out_identity->kind = IC_IDENTITY_ED25519;
    out_identity->impl = impl;
    return IC_AGENT_OK;
}

ic_agent_error_code_t ic_identity_public_key_der(const ic_identity_t *identity,
                                                 uint8_t **out_pubkey,
                                                 size_t   *out_pubkey_len) {
    ic_ed25519_identity_impl_t *impl = NULL;
    int                         der_len = 0;
    uint8_t                    *der = NULL;
    unsigned char              *p = NULL;

    if (out_pubkey == NULL || out_pubkey_len == NULL || identity == NULL) {
        return IC_AGENT_ERR;
    }
    *out_pubkey = NULL;
    *out_pubkey_len = 0;

    if (identity->kind == IC_IDENTITY_ANONYMOUS) {
        return IC_AGENT_OK;
    }

    impl = as_ed25519_impl(identity);
    if (impl == NULL) {
        return IC_AGENT_ERR;
    }

    der_len = i2d_PUBKEY(impl->pkey, NULL);
    if (der_len <= 0) {
        return IC_AGENT_ERR;
    }

    der = (uint8_t *)malloc((size_t)der_len);
    if (der == NULL) {
        return IC_AGENT_ERR;
    }

    p = der;
    if (i2d_PUBKEY(impl->pkey, &p) != der_len) {
        free(der);
        return IC_AGENT_ERR;
    }

    *out_pubkey = der;
    *out_pubkey_len = (size_t)der_len;
    return IC_AGENT_OK;
}

ic_agent_error_code_t
ic_identity_sign_request_id(const ic_identity_t   *identity,
                            const ic_request_id_t *request_id,
                            uint8_t              **out_signature,
                            size_t                *out_signature_len) {
    ic_ed25519_identity_impl_t *impl = NULL;
    EVP_MD_CTX                 *ctx = NULL;
    uint8_t                     signable[43];
    size_t                      sig_len = 0;
    uint8_t                    *sig = NULL;

    if (identity == NULL || request_id == NULL || out_signature == NULL ||
        out_signature_len == NULL) {
        return IC_AGENT_ERR;
    }
    *out_signature = NULL;
    *out_signature_len = 0;

    if (identity->kind == IC_IDENTITY_ANONYMOUS) {
        return IC_AGENT_OK;
    }

    impl = as_ed25519_impl(identity);
    if (impl == NULL) {
        return IC_AGENT_ERR;
    }

    memcpy(signable, "\x0Aic-request", 11);
    memcpy(signable + 11, request_id->hash, 32);

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        return IC_AGENT_ERR;
    }
    if (EVP_DigestSignInit(ctx, NULL, NULL, NULL, impl->pkey) != 1) {
        EVP_MD_CTX_free(ctx);
        return IC_AGENT_ERR;
    }

    if (EVP_DigestSign(ctx, NULL, &sig_len, signable, sizeof(signable)) != 1) {
        EVP_MD_CTX_free(ctx);
        return IC_AGENT_ERR;
    }

    sig = (uint8_t *)malloc(sig_len);
    if (sig == NULL) {
        EVP_MD_CTX_free(ctx);
        return IC_AGENT_ERR;
    }

    if (EVP_DigestSign(ctx, sig, &sig_len, signable, sizeof(signable)) != 1) {
        free(sig);
        EVP_MD_CTX_free(ctx);
        return IC_AGENT_ERR;
    }
    EVP_MD_CTX_free(ctx);

    *out_signature = sig;
    *out_signature_len = sig_len;
    return IC_AGENT_OK;
}

void ic_identity_bytes_free(uint8_t *bytes) { free(bytes); }

void ic_identity_destroy(ic_identity_t *identity) {
    ic_ed25519_identity_impl_t *impl = NULL;

    if (identity == NULL) {
        return;
    }
    if (identity->kind == IC_IDENTITY_ED25519 && identity->impl != NULL) {
        impl = (ic_ed25519_identity_impl_t *)identity->impl;
        EVP_PKEY_free(impl->pkey);
        free(impl);
    }
    identity->kind = IC_IDENTITY_ANONYMOUS;
    identity->impl = NULL;
}
