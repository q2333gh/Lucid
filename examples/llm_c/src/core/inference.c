/*
 * GPT-2 Inference Engine - Platform Independent
 *
 * Unified inference engine that uses storage and platform interfaces
 * to run inference on both native and IC platforms.
 */

#include "inference.h"
#include "../interface/platform.h"
#include "../interface/storage.h"
#include "gguf_loader.h"
#include "model.h"
#include "sampler.h"
#include "tokenizer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LLM_CORE_LOG(engine, ...)                                              \
    do {                                                                       \
        if ((engine) && (engine)->platform &&                                  \
            (engine)->platform->debug_printf) {                                \
            (engine)->platform->debug_printf(__VA_ARGS__);                     \
        }                                                                      \
    } while (0)

int llm_inference_init(llm_inference_engine_t *engine,
                       llm_storage_t          *storage,
                       llm_platform_t         *platform) {
    if (!engine || !storage || !platform) {
        return -1;
    }

    memset(engine, 0, sizeof(llm_inference_engine_t));
    engine->storage = storage;
    engine->platform = platform;
    engine->temperature = 1.0f;
    engine->top_k = 40;
    engine->rng_state = 1337; // Default seed

    return 0;
}

int llm_inference_load_model(llm_inference_engine_t *engine) {
    if (!engine || !engine->storage || !engine->platform) {
        return -1;
    }

    // Load checkpoint
    uint8_t *checkpoint_data = NULL;
    size_t   checkpoint_len = 0;
    if (engine->storage->read_checkpoint(engine->storage->context, "checkpoint",
                                         &checkpoint_data,
                                         &checkpoint_len) != 0) {
        return -1;
    }

    // Allocate model
    engine->model = (GPT2 *)engine->platform->alloc(sizeof(GPT2));
    if (!engine->model) {
        engine->platform->free(checkpoint_data);
        return -1;
    }

    // Build model from buffer
    gpt2_build_from_buffer(engine->model, checkpoint_data, checkpoint_len,
                           engine->platform->alloc,
                           engine->platform->error_exit);

    engine->platform->free(checkpoint_data);

    // Load tokenizer
    uint8_t *vocab_data = NULL;
    size_t   vocab_len = 0;
    uint8_t *merges_data = NULL;
    size_t   merges_len = 0;

    if (engine->storage->read_asset(engine->storage->context, "vocab",
                                    &vocab_data, &vocab_len) != 0) {
        gpt2_free(engine->model, engine->platform->free);
        engine->platform->free(engine->model);
        engine->model = NULL;
        return -1;
    }

    if (engine->storage->read_asset(engine->storage->context, "merges",
                                    &merges_data, &merges_len) != 0) {
        engine->platform->free(vocab_data);
        gpt2_free(engine->model, engine->platform->free);
        engine->platform->free(engine->model);
        engine->model = NULL;
        return -1;
    }

    engine->tokenizer = gpt2_tokenizer_load_from_memory(
        (const char *)vocab_data, vocab_len, (const char *)merges_data,
        merges_len, engine->platform->alloc);

    engine->platform->free(vocab_data);
    engine->platform->free(merges_data);

    if (!engine->tokenizer) {
        gpt2_free(engine->model, engine->platform->free);
        engine->platform->free(engine->model);
        engine->model = NULL;
        return -1;
    }

    // Pre-allocate activation memory with max batch_size=1, max_seq_len=1024
    // This allows inference to work with any sequence length up to max_seq_len
    int B = 1;
    int T = engine->model->config.max_seq_len;
    int C = engine->model->config.channels;
    int L = engine->model->config.num_layers;
    int NH = engine->model->config.num_heads;
    int V = engine->model->config.vocab_size;

    engine->model->batch_size = B;
    engine->model->seq_len = T;
    engine->model->act_sizes[0] = B * T * C;
    engine->model->act_sizes[1] = L * B * T * C;
    engine->model->act_sizes[2] = L * B * T;
    engine->model->act_sizes[3] = L * B * T;
    engine->model->act_sizes[4] = L * B * T * 3 * C;
    engine->model->act_sizes[5] = L * B * T * C;
    engine->model->act_sizes[6] = L * B * NH * T * T;
    engine->model->act_sizes[7] = L * B * NH * T * T;
    engine->model->act_sizes[8] = L * B * T * C;
    engine->model->act_sizes[9] = L * B * T * C;
    engine->model->act_sizes[10] = L * B * T * C;
    engine->model->act_sizes[11] = L * B * T;
    engine->model->act_sizes[12] = L * B * T;
    engine->model->act_sizes[13] = L * B * T * 4 * C;
    engine->model->act_sizes[14] = L * B * T * 4 * C;
    engine->model->act_sizes[15] = L * B * T * C;
    engine->model->act_sizes[16] = L * B * T * C;
    engine->model->act_sizes[17] = B * T * C;
    engine->model->act_sizes[18] = B * T;
    engine->model->act_sizes[19] = B * T;
    engine->model->act_sizes[20] = B * T * V;
    engine->model->act_sizes[21] = B * T * V;
    engine->model->act_sizes[22] = B * T;
    engine->model->act_sizes[23] = L * engine->model->config.max_seq_len * C;
    engine->model->act_sizes[24] = L * engine->model->config.max_seq_len * C;

    engine->model->acts_memory = gpt2_malloc_and_point_activations(
        &engine->model->acts, engine->model->act_sizes,
        engine->platform->alloc);

    if (!engine->model->acts_memory) {
        gpt2_tokenizer_free(engine->tokenizer, engine->platform->free);
        engine->tokenizer = NULL;
        gpt2_free(engine->model, engine->platform->free);
        engine->platform->free(engine->model);
        engine->model = NULL;
        return -1;
    }

    engine->model->inputs = (int *)engine->platform->alloc(B * T * sizeof(int));
    engine->model->targets =
        (int *)engine->platform->alloc(B * T * sizeof(int));
    if (!engine->model->inputs || !engine->model->targets) {
        engine->platform->free(engine->model->acts_memory);
        gpt2_tokenizer_free(engine->tokenizer, engine->platform->free);
        engine->tokenizer = NULL;
        gpt2_free(engine->model, engine->platform->free);
        engine->platform->free(engine->model);
        engine->model = NULL;
        return -1;
    }

    return 0;
}

int llm_inference_load_model_gguf(llm_inference_engine_t *engine) {
    if (!engine || !engine->storage || !engine->platform) {
        LLM_CORE_LOG(engine, "[GGUF] Invalid engine/storage/platform\n");
        return -1;
    }

    LLM_CORE_LOG(engine, "[GGUF] Starting GGUF model load\n");

    // Read GGUF file
    uint8_t *gguf_data = NULL;
    size_t   gguf_len = 0;
    if (!engine->storage->read_gguf) {
        LLM_CORE_LOG(engine, "[GGUF] read_gguf not implemented\n");
        return -1;
    }

    LLM_CORE_LOG(engine, "[GGUF] Calling read_gguf\n");
    if (engine->storage->read_gguf(engine->storage->context, "model",
                                   &gguf_data, &gguf_len) != 0) {
        LLM_CORE_LOG(engine, "[GGUF] Failed to read GGUF file\n");
        return -1;
    }

    LLM_CORE_LOG(engine, "[GGUF] GGUF data read: %zu bytes\n", gguf_len);

    int gguf_in_memory = gguf_is_valid(gguf_data, gguf_len);

    // Parse config
    if (gguf_in_memory) {
        LLM_CORE_LOG(engine, "[GGUF] Parsing config from memory (%zu bytes)\n",
                     gguf_len);
    } else {
        LLM_CORE_LOG(engine, "[GGUF] Parsing config from: %s\n",
                     (char *)gguf_data);
    }
    GPT2Config config;
    if (gguf_parse_config(gguf_data, gguf_len, &config) != 0) {
        LLM_CORE_LOG(engine, "[GGUF] Failed to parse config\n");
        engine->platform->free(gguf_data);
        return -1;
    }
    LLM_CORE_LOG(
        engine,
        "[GGUF] Config parsed: channels=%d, layers=%d, heads=%d, vocab=%d\n",
        config.channels, config.num_layers, config.num_heads,
        config.vocab_size);

    // Allocate model
    LLM_CORE_LOG(engine, "[GGUF] Allocating model structure\n");
    engine->model = (GPT2 *)engine->platform->alloc(sizeof(GPT2));
    if (!engine->model) {
        LLM_CORE_LOG(engine, "[GGUF] Failed to allocate model\n");
        engine->platform->free(gguf_data);
        return -1;
    }

    memset(engine->model, 0, sizeof(GPT2));
    engine->model->config = config;

    // Prepare parameter layout from config
    // Create model_header array: [magic, version, maxT, V, L, NH, C]
    LLM_CORE_LOG(engine, "[GGUF] Preparing parameter layout\n");
    int model_header[7] = {0x67676d66,         1,
                           config.max_seq_len, config.vocab_size,
                           config.num_layers,  config.num_heads,
                           config.channels};
    gpt2_prepare_param_layout(engine->model, model_header);
    LLM_CORE_LOG(engine, "[GGUF] Parameter layout prepared\n");

    // Allocate parameter memory
    LLM_CORE_LOG(engine, "[GGUF] Allocating parameter memory\n");
    engine->model->params_memory = gpt2_malloc_and_point_parameters(
        &engine->model->params, engine->model->param_sizes,
        engine->platform->alloc);

    if (!engine->model->params_memory) {
        LLM_CORE_LOG(engine, "[GGUF] Failed to allocate parameter memory\n");
        engine->platform->free(engine->model);
        engine->model = NULL;
        engine->platform->free(gguf_data);
        return -1;
    }
    LLM_CORE_LOG(engine, "[GGUF] Parameter memory allocated\n");

    // Load weights
    if (gguf_load_weights(gguf_data, gguf_len, engine->model,
                          engine->platform->alloc) != 0) {
        gpt2_free(engine->model, engine->platform->free);
        engine->platform->free(engine->model);
        engine->model = NULL;
        engine->platform->free(gguf_data);
        return -1;
    }

    // Extract tokenizer from GGUF (fallback to external assets if missing)
    uint8_t *vocab_data = NULL;
    size_t   vocab_len = 0;
    uint8_t *merges_data = NULL;
    size_t   merges_len = 0;

    int tok_ok = gguf_extract_tokenizer(gguf_data, gguf_len, &vocab_data,
                                        &vocab_len, &merges_data, &merges_len,
                                        engine->platform->alloc);
    engine->platform->free(gguf_data);
    gguf_data = NULL;

    if (tok_ok != 0) {
        if (engine->storage->read_asset(engine->storage->context, "vocab",
                                        &vocab_data, &vocab_len) != 0) {
            gpt2_free(engine->model, engine->platform->free);
            engine->platform->free(engine->model);
            engine->model = NULL;
            return -1;
        }
        if (engine->storage->read_asset(engine->storage->context, "merges",
                                        &merges_data, &merges_len) != 0) {
            engine->platform->free(vocab_data);
            gpt2_free(engine->model, engine->platform->free);
            engine->platform->free(engine->model);
            engine->model = NULL;
            return -1;
        }
    }

    // Load tokenizer
    engine->tokenizer = gpt2_tokenizer_load_from_memory(
        (const char *)vocab_data, vocab_len, (const char *)merges_data,
        merges_len, engine->platform->alloc);

    engine->platform->free(vocab_data);
    engine->platform->free(merges_data);

    if (!engine->tokenizer) {
        gpt2_free(engine->model, engine->platform->free);
        engine->platform->free(engine->model);
        engine->model = NULL;
        return -1;
    }

    // Pre-allocate activation memory with max batch_size=1, max_seq_len
    int B = 1;
    int T = engine->model->config.max_seq_len;
    int C = engine->model->config.channels;
    int L = engine->model->config.num_layers;
    int NH = engine->model->config.num_heads;
    int V = engine->model->config.vocab_size;

    engine->model->batch_size = B;
    engine->model->seq_len = T;
    engine->model->act_sizes[0] = B * T * C;
    engine->model->act_sizes[1] = L * B * T * C;
    engine->model->act_sizes[2] = L * B * T;
    engine->model->act_sizes[3] = L * B * T;
    engine->model->act_sizes[4] = L * B * T * 3 * C;
    engine->model->act_sizes[5] = L * B * T * C;
    engine->model->act_sizes[6] = L * B * NH * T * T;
    engine->model->act_sizes[7] = L * B * NH * T * T;
    engine->model->act_sizes[8] = L * B * T * C;
    engine->model->act_sizes[9] = L * B * T * C;
    engine->model->act_sizes[10] = L * B * T * C;
    engine->model->act_sizes[11] = L * B * T;
    engine->model->act_sizes[12] = L * B * T;
    engine->model->act_sizes[13] = L * B * T * 4 * C;
    engine->model->act_sizes[14] = L * B * T * 4 * C;
    engine->model->act_sizes[15] = L * B * T * C;
    engine->model->act_sizes[16] = L * B * T * C;
    engine->model->act_sizes[17] = B * T * C;
    engine->model->act_sizes[18] = B * T;
    engine->model->act_sizes[19] = B * T;
    engine->model->act_sizes[20] = B * T * V;
    engine->model->act_sizes[21] = B * T * V;
    engine->model->act_sizes[22] = B * T;
    engine->model->act_sizes[23] = L * engine->model->config.max_seq_len * C;
    engine->model->act_sizes[24] = L * engine->model->config.max_seq_len * C;

    engine->model->acts_memory = gpt2_malloc_and_point_activations(
        &engine->model->acts, engine->model->act_sizes,
        engine->platform->alloc);

    if (!engine->model->acts_memory) {
        gpt2_tokenizer_free(engine->tokenizer, engine->platform->free);
        engine->tokenizer = NULL;
        gpt2_free(engine->model, engine->platform->free);
        engine->platform->free(engine->model);
        engine->model = NULL;
        return -1;
    }

    engine->model->inputs = (int *)engine->platform->alloc(B * T * sizeof(int));
    engine->model->targets =
        (int *)engine->platform->alloc(B * T * sizeof(int));
    if (!engine->model->inputs || !engine->model->targets) {
        engine->platform->free(engine->model->acts_memory);
        gpt2_tokenizer_free(engine->tokenizer, engine->platform->free);
        engine->tokenizer = NULL;
        gpt2_free(engine->model, engine->platform->free);
        engine->platform->free(engine->model);
        engine->model = NULL;
        return -1;
    }

    return 0;
}

int llm_inference_generate(llm_inference_engine_t *engine,
                           const int              *prompt_tokens,
                           int                     prompt_len,
                           int                     max_new_tokens,
                           int                   **out_tokens) {
    if (!engine || !engine->model || !prompt_tokens || prompt_len <= 0) {
        return -1;
    }

    int max_total = engine->model->config.max_seq_len;
    if (prompt_len >= max_total) {
        return -1;
    }
    if (max_new_tokens > max_total - prompt_len) {
        max_new_tokens = max_total - prompt_len;
    }

    // Allocate output buffer
    int *generated = (int *)engine->platform->alloc(
        (prompt_len + max_new_tokens) * sizeof(int));
    if (!generated) {
        return -1;
    }

    // Copy prompt
    memcpy(generated, prompt_tokens, prompt_len * sizeof(int));
    int total_len = prompt_len;

    // Prime KV cache with prompt tokens
    engine->model->cache_pos = 0;
    for (int i = 0; i < prompt_len; i++) {
        gpt2_forward_single(engine->model, generated[i],
                            engine->model->cache_pos,
                            engine->platform->error_exit);
        engine->model->cache_pos++;
    }

    // Generate tokens
    for (int i = 0; i < max_new_tokens; i++) {
        // Get logits for last position
        int    last_pos = engine->model->cache_pos - 1;
        float *logits = engine->model->acts.logits +
                        last_pos * engine->model->config.vocab_size;

        // Apply temperature
        if (engine->temperature != 1.0f) {
            for (int j = 0; j < engine->model->config.vocab_size; j++) {
                logits[j] /= engine->temperature;
            }
        }

        // Apply top-k
        gpt2_top_k_filter(logits, engine->model->config.vocab_size,
                          engine->top_k);

        // Softmax
        float *probs = (float *)engine->platform->alloc(
            engine->model->config.vocab_size * sizeof(float));
        if (!probs) {
            engine->platform->free(generated);
            return -1;
        }
        gpt2_softmax_vec(logits, probs, engine->model->config.vocab_size);

        // Sample
        float coin = gpt2_rng_f32(&engine->rng_state);
        int   next_token =
            gpt2_sample_mult(probs, engine->model->config.vocab_size, coin);

        engine->platform->free(probs);

        generated[total_len++] = next_token;

        // Stop on EOT token
        if (next_token == 50256) { // GPT2_EOT
            break;
        }

        // Advance KV cache with the new token
        gpt2_forward_single(engine->model, next_token, engine->model->cache_pos,
                            engine->platform->error_exit);
        engine->model->cache_pos++;
    }

    *out_tokens = generated;
    return total_len - prompt_len; // Return number of new tokens
}

void llm_inference_cleanup(llm_inference_engine_t *engine) {
    if (!engine) {
        return;
    }

    if (engine->model) {
        gpt2_free(engine->model, engine->platform->free);
        engine->platform->free(engine->model);
        engine->model = NULL;
    }

    if (engine->tokenizer) {
        gpt2_tokenizer_free(engine->tokenizer, engine->platform->free);
        engine->tokenizer = NULL;
    }

    if (engine->sequence) {
        engine->platform->free(engine->sequence);
        engine->sequence = NULL;
    }
}
