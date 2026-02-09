#pragma once

#include "icp_agent/error.h"
#include "icp_agent/types.h"

ic_agent_error_code_t
ic_compute_request_id(const ic_envelope_content_t *content,
                      ic_request_id_t             *out_id);
