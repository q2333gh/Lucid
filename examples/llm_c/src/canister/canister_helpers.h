#ifndef EXAMPLES_LLMC_CANISTER_HELPERS_H
#define EXAMPLES_LLMC_CANISTER_HELPERS_H

#define LLM_C_CANISTER_BUILD

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ic_c_sdk.h"
#include "ic_storage.h"
#include "idl/cdk_alloc.h"

#define GPT2_EOT 50256
#define MAX_ASSETS 8
#define MAX_ASSET_NAME 64
#define DEFAULT_MAX_NEW_TOKENS 64
#define DEFAULT_TEMPERATURE 1.0f
#define DEFAULT_TOP_K 40
#define DEFAULT_PROMPT "hello"
#define LLM_C_ASSETS_TESTING_STUB_REPLY                                        \
    "<llm_c inference disabled in asset tests; load real assets to run>"

void  trap_msg(const char *msg);
void  write_asset_data(const char *name, const uint8_t *data, size_t len);
void  read_asset_blob(const char *name, uint8_t **out_data, size_t *out_len);
char *run_inference(const char *prompt);
char *run_inference_limited(const char *prompt, int max_new);
char     *
run_inference_tokens(const int *prompt_tokens, int prompt_len, int max_new);
int read_asset_tokens_u16(const char *name,
                          int         offset,
                          int         count,
                          int       **out_tokens);
void llm_c_infer_state_init_from_tokens(const int *prompt_tokens,
                                        int        prompt_len,
                                        int        max_new);
void llm_c_infer_state_step(uint32_t steps);
void llm_c_reset_inference_cache(void);
int  llm_c_asset_exists(const char *name);
int  llm_c_checkpoint_header_ok(void);
int  llm_c_checkpoint_complete(void);
void require_assets_loaded(void);

// Test helpers
void   llm_c_reset_asset_state(void);
size_t llm_c_asset_count(void);

// Verification helpers
uint32_t llm_c_compute_asset_hash(const char *name);
void     llm_c_get_asset_sample(const char *name,
                                uint32_t    offset,
                                uint32_t    length,
                                uint8_t   **out_data,
                                size_t     *out_len);

#endif // EXAMPLES_LLMC_CANISTER_HELPERS_H
