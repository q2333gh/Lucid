#include "canister_helpers.h"
#include "ic_simple.h"
IC_CANDID_EXPORT_DID()

#include "idl/candid.h"

#define STORIES_ASSET_NAME "stories"
#define DEFAULT_STORIES_OFFSET 0
#define DEFAULT_STORIES_LENGTH 64
#define DEFAULT_STORIES_MAX_NEW 1

IC_API_UPDATE(load_asset, "(text, blob) -> ()") {
    char    *name = NULL;
    uint8_t *payload = NULL;
    size_t   payload_len = 0;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_TEXT(&name);
    IC_ARG_BLOB(&payload, &payload_len);
    IC_API_PARSE_END();

    if (!name || payload_len == 0) {
        trap_msg("load_asset: invalid args");
    }
    char   asset_name[MAX_ASSET_NAME];
    size_t copy_len =
        strlen(name) < MAX_ASSET_NAME - 1 ? strlen(name) : MAX_ASSET_NAME - 1;
    memcpy(asset_name, name, copy_len);
    asset_name[copy_len] = '\0';

    write_asset_data(asset_name, payload, payload_len);
    IC_API_REPLY_EMPTY();
}

IC_API_QUERY(infer_hello, "() -> (text)") {
    ic_api_debug_printf("infer_hello start: %s", DEFAULT_PROMPT);
    char *reply = run_inference(DEFAULT_PROMPT);
    IC_API_REPLY_TEXT(reply);
    free(reply);
}

IC_API_UPDATE(infer_hello_update, "() -> (text)") {
    ic_api_debug_printf("infer_hello_update start: %s", DEFAULT_PROMPT);
    char *reply = run_inference(DEFAULT_PROMPT);
    IC_API_REPLY_TEXT(reply);
    free(reply);
}

IC_API_UPDATE(infer_hello_update_limited, "(nat32) -> (text)") {
    uint64_t max_new = 0;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_NAT(&max_new);
    IC_API_PARSE_END();

    ic_api_debug_printf("infer_hello_update_limited start: %s max_new=%u",
                        DEFAULT_PROMPT, (unsigned)max_new);
    char *reply = run_inference_limited(DEFAULT_PROMPT, (int)max_new);
    IC_API_REPLY_TEXT(reply);
    free(reply);
}

IC_API_UPDATE(infer_stories_update, "() -> (text)") {
    int *tokens = NULL;
    int  prompt_len =
        read_asset_tokens_u16(STORIES_ASSET_NAME, DEFAULT_STORIES_OFFSET,
                              DEFAULT_STORIES_LENGTH, &tokens);
    char *reply =
        run_inference_tokens(tokens, prompt_len, DEFAULT_STORIES_MAX_NEW);
    free(tokens);
    IC_API_REPLY_TEXT(reply);
    free(reply);
}

IC_API_UPDATE(infer_stories_update_limited, "(nat32, nat32, nat32) -> (text)") {
    uint64_t offset = 0;
    uint64_t length = 0;
    uint64_t max_new = 0;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_NAT(&offset);
    IC_ARG_NAT(&length);
    IC_ARG_NAT(&max_new);
    IC_API_PARSE_END();

    if (offset > INT_MAX || length > INT_MAX || max_new > INT_MAX)
        trap_msg("infer_stories_update_limited: args too large");
    int  *tokens = NULL;
    int   prompt_len = read_asset_tokens_u16(STORIES_ASSET_NAME, (int)offset,
                                             (int)length, &tokens);
    char *reply = run_inference_tokens(tokens, prompt_len, (int)max_new);
    free(tokens);
    IC_API_REPLY_TEXT(reply);
    free(reply);
}

IC_API_UPDATE(infer_stories_step_init, "(nat32, nat32, nat32) -> (text)") {
    uint64_t offset = 0;
    uint64_t length = 0;
    uint64_t max_new = 0;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_NAT(&offset);
    IC_ARG_NAT(&length);
    IC_ARG_NAT(&max_new);
    IC_API_PARSE_END();

    if (offset > INT_MAX || length > INT_MAX || max_new > INT_MAX)
        trap_msg("infer_stories_step_init: args too large");
    int *tokens = NULL;
    int  prompt_len = read_asset_tokens_u16(STORIES_ASSET_NAME, (int)offset,
                                            (int)length, &tokens);
    llm_c_infer_state_init_from_tokens(tokens, prompt_len, (int)max_new);
    free(tokens);
    IC_API_REPLY_TEXT("submitted");
}

IC_API_UPDATE(infer_stories_step, "(nat32) -> (text)") {
    uint64_t steps = 0;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_NAT(&steps);
    IC_API_PARSE_END();

    if (steps > UINT32_MAX)
        trap_msg("infer_stories_step: steps too large");
    llm_c_infer_state_step((uint32_t)steps);
    IC_API_REPLY_TEXT("submitted");
}

IC_API_UPDATE(reset_assets, "() -> ()") {
    llm_c_reset_asset_state();
    IC_API_REPLY_EMPTY();
}

IC_API_QUERY(hello, "() -> (text)") {
    IC_API_REPLY_TEXT("hello world from llm_c");
}

IC_API_QUERY(assets_loaded, "() -> (text)") {
    require_assets_loaded();
    IC_API_REPLY_TEXT("assets loaded");
}

IC_API_QUERY(checkpoint_valid, "() -> (nat)") {
    IC_API_REPLY_NAT(llm_c_checkpoint_header_ok() ? 1 : 0);
}

IC_API_QUERY(checkpoint_complete, "() -> (nat)") {
    IC_API_REPLY_NAT(llm_c_checkpoint_complete() ? 1 : 0);
}

IC_API_QUERY(asset_exists, "(text) -> (nat)") {
    char *name = NULL;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_TEXT(&name);
    IC_API_PARSE_END();
    IC_API_REPLY_NAT(llm_c_asset_exists(name) ? 1 : 0);
}

IC_API_QUERY(compute_asset_hash, "(text) -> (nat)") {
    char *name = NULL;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_TEXT(&name);
    IC_API_PARSE_END();
    uint32_t hash = llm_c_compute_asset_hash(name);
    uint64_t result = (uint64_t)hash;
    IC_API_REPLY_NAT(result);
}

IC_API_QUERY(get_asset_sample, "(text, nat32, nat32) -> (blob)") {
    char    *name = NULL;
    uint64_t offset = 0;
    uint64_t length = 0;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_TEXT(&name);
    IC_ARG_NAT(&offset);
    IC_ARG_NAT(&length);
    IC_API_PARSE_END();

    uint8_t *data = NULL;
    size_t   len = 0;
    llm_c_get_asset_sample(name, (uint32_t)offset, (uint32_t)length, &data,
                           &len);
    IC_API_REPLY_BLOB(data, len);
    if (data)
        free(data);
}

/*
Measured the largest single-chunk payload that load_asset can accept: wrote
find_max_chunk.py (and reran it interactively) to hit the canister with
incrementally larger blobs, then honed in with smaller steps. Sizes up through
262 119 bytes succeed, while 262 120 bytes already traps (ic_api_init fails).
That makes 262 119 B the largest working chunk before the IC stack rejects the
request. Because the canister traps at ~262 kB, keep each upload chunk below
that (the existing DEFAULT_CHUNK_SIZE = 64 KiB is safely conservative). If you
need to drive higher throughput, you now know the hard limit and can tune the
chunking code accordingly. Tests run:

find_max_chunk.py
Manual incremental scripts (sizes around 262,000 B) executed via the agent to
confirm the breaking point.
TODO find how big cdk-rs can handle. to support larger chunk size.
*/
