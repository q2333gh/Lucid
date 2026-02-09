#pragma once

#include <stddef.h>
#include <stdint.h>

#include "icp_agent/error.h"
#include "icp_agent/types.h"

typedef struct {
    char *replica_url;
} ic_agent_t;

const char *ic_agent_version(void);

ic_agent_error_code_t ic_agent_init(ic_agent_t *agent, const char *replica_url);

void ic_agent_destroy(ic_agent_t *agent);

ic_agent_error_code_t ic_agent_query(ic_agent_t    *agent,
                                     const uint8_t *canister_id,
                                     size_t         canister_id_len,
                                     const char    *method_name,
                                     const uint8_t *arg,
                                     size_t         arg_len,
                                     uint8_t      **out_reply,
                                     size_t        *out_reply_len);

ic_agent_error_code_t ic_agent_update(ic_agent_t      *agent,
                                      const uint8_t   *canister_id,
                                      size_t           canister_id_len,
                                      const char      *method_name,
                                      const uint8_t   *arg,
                                      size_t           arg_len,
                                      ic_request_id_t *out_request_id);

ic_agent_error_code_t ic_agent_wait(ic_agent_t            *agent,
                                    const ic_request_id_t *request_id,
                                    const uint8_t *effective_canister_id,
                                    size_t         effective_canister_id_len,
                                    uint8_t      **out_reply,
                                    size_t        *out_reply_len);

void ic_agent_bytes_free(uint8_t *bytes);
