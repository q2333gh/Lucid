/*
 * IC Canister Main Entry Point
 *
 * Platform-abstracted canister with legacy-compatible exports.
 */

#include "../../core/inference.h"
#include "../../core/model.h"
#include "../../core/sampler.h"
#include "../../core/tokenizer.h"
#include "../../platform/ic/asset_manager.h"
#include "../../platform/ic/platform_impl.h"
#include "../../platform/ic/storage_impl.h"
#include "ic_c_sdk.h"
#include "ic_storage.h"
#include "idl/candid.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

IC_CANDID_EXPORT_DID()

#define DEFAULT_PROMPT "hello"
#define DEFAULT_MAX_NEW_TOKENS 64
#define DEFAULT_STORIES_OFFSET 0
#define DEFAULT_STORIES_LENGTH 64
#define DEFAULT_STORIES_MAX_NEW 1
#define GPT2_EOT 50256

static const char *const kRequiredAssets[] = {"checkpoint", "vocab", "merges"};
static const size_t      kRequiredAssetCount =
    sizeof(kRequiredAssets) / sizeof(kRequiredAssets[0]);

static llm_storage_t         *g_storage = NULL;
static llm_platform_t        *g_platform = NULL;
static llm_inference_engine_t g_engine;
static int                    g_engine_ready = 0;
static int                    g_model_loaded = 0;
static int                    g_use_gguf = 0;

static int *g_step_sequence = NULL;
static int  g_step_len = 0;
static int  g_step_max_len = 0;
static int  g_step_done = 0;

static void trap_msg(const char *msg) { ic_api_trap(msg); }

static void ensure_storage_platform(void) {
    if (!g_platform) {
        g_platform = llm_platform_create_ic();
        if (!g_platform) {
            trap_msg("platform init failed");
        }
    }

    if (!g_storage) {
        g_storage = llm_storage_create_ic();
        if (!g_storage) {
            trap_msg("storage init failed");
        }
    }

    if (!g_engine_ready) {
        if (llm_inference_init(&g_engine, g_storage, g_platform) != 0) {
            trap_msg("engine init failed");
        }
        g_engine.rng_state = g_platform->get_random_seed();
        g_engine_ready = 1;
    }
}

static ic_storage_ctx_t *get_asset_ctx(void) {
    ensure_storage_platform();
    return (ic_storage_ctx_t *)g_storage->context;
}

static asset_entry_t *lookup_asset(const char *name) {
    ic_storage_ctx_t *ctx = get_asset_ctx();
    return ic_asset_lookup(ctx, name);
}

static int asset_exists_impl(const char *name) {
    asset_entry_t *entry = lookup_asset(name);
    if (!entry) {
        return 0;
    }
    return entry->length > 0 ? 1 : 0;
}

static void reset_step_state(void) {
    if (g_step_sequence) {
        free(g_step_sequence);
    }
    g_step_sequence = NULL;
    g_step_len = 0;
    g_step_max_len = 0;
    g_step_done = 0;
}

static void reset_engine(void) {
    if (g_engine_ready) {
        llm_inference_cleanup(&g_engine);
    }
    g_engine_ready = 0;
    g_model_loaded = 0;
    g_use_gguf = 0;
    reset_step_state();
    if (g_storage) {
        llm_storage_free_ic(g_storage);
        g_storage = NULL;
    }
    if (g_platform) {
        llm_platform_free_ic(g_platform);
        g_platform = NULL;
    }
}

static void require_assets_loaded(void) {
    if (asset_exists_impl("model")) {
        return;
    }
    for (size_t i = 0; i < kRequiredAssetCount; i++) {
        if (!asset_exists_impl(kRequiredAssets[i])) {
            trap_msg("missing required asset");
        }
    }
}

static void ensure_model_loaded(void) {
    ensure_storage_platform();
    if (g_model_loaded) {
        return;
    }

    g_use_gguf = asset_exists_impl("model");
    if (!g_use_gguf) {
        require_assets_loaded();
        if (llm_inference_load_model(&g_engine) != 0) {
            trap_msg("checkpoint load failed");
        }
    } else {
        if (llm_inference_load_model_gguf(&g_engine) != 0) {
            trap_msg("gguf load failed");
        }
    }
    g_model_loaded = 1;
}

static char *alloc_dup(const char *src) {
    size_t len = src ? strlen(src) : 0;
    char  *dst = (char *)g_platform->alloc(len + 1);
    if (!dst) {
        return NULL;
    }
    if (len) {
        memcpy(dst, src, len);
    }
    dst[len] = '\0';
    return dst;
}

static char *run_inference_prompt(const char *prompt, int max_new) {
    ensure_model_loaded();
    int  prompt_len = 0;
    int *prompt_tokens = gpt2_tokenizer_encode(g_engine.tokenizer, prompt,
                                               &prompt_len, g_platform->alloc);
    if (!prompt_tokens || prompt_len <= 0) {
        if (prompt_tokens) {
            g_platform->free(prompt_tokens);
        }
        trap_msg("failed to encode prompt");
    }

    int *generated = NULL;
    int  new_tokens = llm_inference_generate(&g_engine, prompt_tokens,
                                             prompt_len, max_new, &generated);
    g_platform->free(prompt_tokens);
    if (new_tokens < 0 || !generated) {
        trap_msg("inference failed");
    }

    int   total_len = prompt_len + new_tokens;
    char *decoded = gpt2_tokenizer_decode(g_engine.tokenizer, generated,
                                          total_len, g_platform->alloc);
    g_platform->free(generated);
    if (!decoded) {
        decoded = alloc_dup("<decode failed>");
    }
    if (!decoded) {
        trap_msg("decode allocation failed");
    }
    return decoded;
}

static char *
run_inference_tokens(const int *prompt_tokens, int prompt_len, int max_new) {
    ensure_model_loaded();
    if (!prompt_tokens || prompt_len <= 0) {
        trap_msg("invalid prompt tokens");
    }

    int *generated = NULL;
    int  new_tokens = llm_inference_generate(&g_engine, prompt_tokens,
                                             prompt_len, max_new, &generated);
    if (new_tokens < 0 || !generated) {
        trap_msg("inference failed");
    }

    int   total_len = prompt_len + new_tokens;
    char *decoded = gpt2_tokenizer_decode(g_engine.tokenizer, generated,
                                          total_len, g_platform->alloc);
    g_platform->free(generated);
    if (!decoded) {
        decoded = alloc_dup("<decode failed>");
    }
    if (!decoded) {
        trap_msg("decode allocation failed");
    }
    return decoded;
}

static int sample_next_token(float *logits) {
    if (g_engine.temperature != 1.0f) {
        for (int i = 0; i < g_engine.model->config.vocab_size; i++) {
            logits[i] /= g_engine.temperature;
        }
    }
    gpt2_top_k_filter(logits, g_engine.model->config.vocab_size,
                      g_engine.top_k);

    float *probs = (float *)g_platform->alloc(
        g_engine.model->config.vocab_size * sizeof(float));
    if (!probs) {
        trap_msg("prob allocation failed");
    }
    gpt2_softmax_vec(logits, probs, g_engine.model->config.vocab_size);
    float coin = gpt2_rng_f32(&g_engine.rng_state);
    int   token =
        gpt2_sample_mult(probs, g_engine.model->config.vocab_size, coin);
    g_platform->free(probs);
    return token;
}

static void
step_inference_init(const int *prompt_tokens, int prompt_len, int max_new) {
    if (!prompt_tokens || prompt_len <= 0) {
        trap_msg("infer_state_init: invalid prompt");
    }
    if (max_new < 0) {
        max_new = 0;
    }
    if (prompt_len > g_engine.model->config.max_seq_len) {
        trap_msg("infer_state_init: prompt too long");
    }
    if (prompt_len + max_new > g_engine.model->config.max_seq_len) {
        max_new = g_engine.model->config.max_seq_len - prompt_len;
    }

    reset_step_state();
    g_step_max_len = prompt_len + max_new;
    g_step_sequence =
        (int *)g_platform->alloc((size_t)g_step_max_len * sizeof(int));
    if (!g_step_sequence) {
        trap_msg("infer_state_init: allocation failed");
    }

    memcpy(g_step_sequence, prompt_tokens, (size_t)prompt_len * sizeof(int));
    if (g_step_max_len > prompt_len) {
        memset(g_step_sequence + prompt_len, 0,
               (size_t)(g_step_max_len - prompt_len) * sizeof(int));
    }
    g_step_len = prompt_len;
    g_step_done = 0;

    g_engine.model->cache_pos = 0;
    for (int i = 0; i < prompt_len; i++) {
        gpt2_forward_single(g_engine.model, g_step_sequence[i],
                            g_engine.model->cache_pos, g_platform->error_exit);
        g_engine.model->cache_pos++;
    }
}

static void step_inference(uint32_t steps) {
    if (!g_step_sequence || g_step_len <= 0) {
        trap_msg("infer_state_step: not initialized");
    }
    if (steps == 0) {
        steps = 1;
    }
    if (g_step_done) {
        return;
    }

    for (uint32_t step = 0; step < steps; step++) {
        if (g_step_len >= g_step_max_len) {
            g_step_done = 1;
            break;
        }

        int    last_pos = g_engine.model->cache_pos - 1;
        float *logits = g_engine.model->acts.logits +
                        last_pos * g_engine.model->config.vocab_size;
        int next = sample_next_token(logits);

        g_step_sequence[g_step_len++] = next;
        if (next == GPT2_EOT) {
            g_step_done = 1;
            break;
        }

        gpt2_forward_single(g_engine.model, next, g_engine.model->cache_pos,
                            g_platform->error_exit);
        g_engine.model->cache_pos++;
    }
}

static char *step_inference_get_text(void) {
    if (!g_step_sequence || g_step_len <= 0) {
        trap_msg("infer_state_get: not initialized");
    }
    ensure_model_loaded();
    char *decoded = gpt2_tokenizer_decode(g_engine.tokenizer, g_step_sequence,
                                          g_step_len, g_platform->alloc);
    if (!decoded) {
        decoded = alloc_dup("<decode failed>");
    }
    if (!decoded) {
        trap_msg("decode allocation failed");
    }
    return decoded;
}

static int checkpoint_valid_impl(void) {
    asset_entry_t *entry = lookup_asset("checkpoint");
    if (!entry || entry->length < (int64_t)(sizeof(int) * 2)) {
        return 0;
    }
    int header[2] = {0};
    ic_stable_read((uint8_t *)header, entry->offset, (int64_t)sizeof(header));
    return (header[0] == 20240326 && header[1] == 1) ? 1 : 0;
}

static int checkpoint_complete_impl(void) {
    asset_entry_t *entry = lookup_asset("checkpoint");
    if (!entry || entry->length < (int64_t)(sizeof(int) * 256)) {
        return 0;
    }
    int model_header[256];
    ic_stable_read((uint8_t *)model_header, entry->offset,
                   (int64_t)sizeof(model_header));
    if (model_header[0] != 20240326 || model_header[1] != 1) {
        return 0;
    }
    GPT2   model;
    size_t num_parameters = gpt2_prepare_param_layout(&model, model_header);
    size_t params_bytes = num_parameters * sizeof(float);
    size_t expected = sizeof(model_header) + params_bytes;
    if ((uint64_t)entry->length < expected) {
        return 0;
    }
    return 1;
}

static uint32_t compute_asset_hash_impl(const char *name) {
    asset_entry_t *entry = lookup_asset(name);
    if (!entry || entry->length <= 0) {
        return 0;
    }

    uint32_t     hash = 5381;
    size_t       len = (size_t)entry->length;
    const size_t chunk_size = 65536;
    uint8_t     *buffer = (uint8_t *)malloc(chunk_size);
    if (!buffer) {
        return 0;
    }

    int64_t offset = entry->offset;
    while (len > 0) {
        size_t to_read = (len < chunk_size) ? len : chunk_size;
        ic_stable_read(buffer, offset, (int64_t)to_read);
        for (size_t i = 0; i < to_read; i++) {
            hash = ((hash << 5) + hash) + buffer[i];
        }
        offset += (int64_t)to_read;
        len -= to_read;
    }

    free(buffer);
    return hash;
}

static void get_asset_sample_impl(const char *name,
                                  uint32_t    req_offset,
                                  uint32_t    req_length,
                                  uint8_t   **out_data,
                                  size_t     *out_len) {
    asset_entry_t *entry = lookup_asset(name);
    if (!entry || entry->length <= 0) {
        *out_data = NULL;
        *out_len = 0;
        return;
    }

    size_t  asset_len = (size_t)entry->length;
    int64_t byte_offset = entry->offset + (int64_t)req_offset;
    if (req_offset >= asset_len) {
        *out_data = NULL;
        *out_len = 0;
        return;
    }

    size_t   available = asset_len - req_offset;
    size_t   to_read = (req_length < available) ? req_length : available;
    uint8_t *buffer = (uint8_t *)malloc(to_read);
    if (!buffer) {
        *out_data = NULL;
        *out_len = 0;
        return;
    }

    ic_stable_read(buffer, byte_offset, (int64_t)to_read);
    *out_data = buffer;
    *out_len = to_read;
}

IC_API_UPDATE(load_asset, "(text, blob) -> ()") {
    char    *name = NULL;
    uint8_t *data = NULL;
    size_t   len = 0;

    IC_API_PARSE_BEGIN(api);
    IC_ARG_TEXT(&name);
    IC_ARG_BLOB(&data, &len);
    IC_API_PARSE_END();

    if (!name || len == 0) {
        trap_msg("load_asset: invalid args");
    }

    ic_storage_ctx_t *ctx = get_asset_ctx();
    ic_asset_write(ctx, name, data, len);

    if (g_model_loaded && g_engine_ready) {
        llm_inference_cleanup(&g_engine);
    }
    g_model_loaded = 0;
    reset_step_state();
    IC_API_REPLY_EMPTY();
}

IC_API_QUERY(infer_hello, "() -> (text)") {
    char *reply = run_inference_prompt(DEFAULT_PROMPT, DEFAULT_MAX_NEW_TOKENS);
    IC_API_REPLY_TEXT(reply);
    g_platform->free(reply);
}

IC_API_UPDATE(infer_hello_update, "() -> (text)") {
    char *reply = run_inference_prompt(DEFAULT_PROMPT, DEFAULT_MAX_NEW_TOKENS);
    IC_API_REPLY_TEXT(reply);
    g_platform->free(reply);
}

IC_API_UPDATE(infer_hello_update_limited, "(nat32) -> (text)") {
    uint64_t max_new = 0;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_NAT(&max_new);
    IC_API_PARSE_END();

    if (max_new > INT_MAX) {
        trap_msg("infer_hello_update_limited: max_new too large");
    }
    char *reply = run_inference_prompt(DEFAULT_PROMPT, (int)max_new);
    IC_API_REPLY_TEXT(reply);
    g_platform->free(reply);
}

IC_API_UPDATE(infer_stories_update, "() -> (text)") {
    ensure_model_loaded();
    int *tokens = NULL;
    if (g_storage->read_tokens(g_storage->context, "stories",
                               DEFAULT_STORIES_OFFSET, DEFAULT_STORIES_LENGTH,
                               &tokens) != 0) {
        trap_msg("infer_stories_update: missing stories");
    }
    char *reply = run_inference_tokens(tokens, DEFAULT_STORIES_LENGTH,
                                       DEFAULT_STORIES_MAX_NEW);
    g_platform->free(tokens);
    IC_API_REPLY_TEXT(reply);
    g_platform->free(reply);
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

    if (offset > INT_MAX || length > INT_MAX || max_new > INT_MAX) {
        trap_msg("infer_stories_update_limited: args too large");
    }

    ensure_model_loaded();
    int *tokens = NULL;
    if (g_storage->read_tokens(g_storage->context, "stories", (int)offset,
                               (int)length, &tokens) != 0) {
        trap_msg("infer_stories_update_limited: missing stories");
    }
    char *reply = run_inference_tokens(tokens, (int)length, (int)max_new);
    g_platform->free(tokens);
    IC_API_REPLY_TEXT(reply);
    g_platform->free(reply);
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

    if (offset > INT_MAX || length > INT_MAX || max_new > INT_MAX) {
        trap_msg("infer_stories_step_init: args too large");
    }

    ensure_model_loaded();
    int *tokens = NULL;
    if (g_storage->read_tokens(g_storage->context, "stories", (int)offset,
                               (int)length, &tokens) != 0) {
        trap_msg("infer_stories_step_init: missing stories");
    }
    step_inference_init(tokens, (int)length, (int)max_new);
    g_platform->free(tokens);
    IC_API_REPLY_TEXT("submitted");
}

IC_API_UPDATE(infer_stories_step, "(nat32) -> (text)") {
    uint64_t steps = 0;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_NAT(&steps);
    IC_API_PARSE_END();

    if (steps > UINT32_MAX) {
        trap_msg("infer_stories_step: steps too large");
    }
    ensure_model_loaded();
    step_inference((uint32_t)steps);
    IC_API_REPLY_TEXT("submitted");
}

IC_API_QUERY(infer_stories_step_get, "() -> (text)") {
    char *reply = step_inference_get_text();
    IC_API_REPLY_TEXT(reply);
    g_platform->free(reply);
}

IC_API_UPDATE(reset_assets, "() -> ()") {
    reset_engine();
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
    IC_API_REPLY_NAT(checkpoint_valid_impl() ? 1 : 0);
}

IC_API_QUERY(checkpoint_complete, "() -> (nat)") {
    IC_API_REPLY_NAT(checkpoint_complete_impl() ? 1 : 0);
}

IC_API_QUERY(asset_exists, "(text) -> (nat)") {
    char *name = NULL;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_TEXT(&name);
    IC_API_PARSE_END();
    IC_API_REPLY_NAT(asset_exists_impl(name) ? 1 : 0);
}

IC_API_QUERY(compute_asset_hash, "(text) -> (nat)") {
    char *name = NULL;
    IC_API_PARSE_BEGIN(api);
    IC_ARG_TEXT(&name);
    IC_API_PARSE_END();
    uint32_t hash = compute_asset_hash_impl(name);
    IC_API_REPLY_NAT((uint64_t)hash);
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
    get_asset_sample_impl(name, (uint32_t)offset, (uint32_t)length, &data,
                          &len);
    IC_API_REPLY_BLOB(data, len);
    if (data) {
        free(data);
    }
}
