#include "icp_agent/identity.h"

ic_agent_error_code_t ic_identity_anonymous_init(ic_identity_t *out_identity) {
    if (out_identity == NULL) {
        return IC_AGENT_ERR;
    }
    out_identity->kind = IC_IDENTITY_ANONYMOUS;
    out_identity->impl = NULL;
    return IC_AGENT_OK;
}
