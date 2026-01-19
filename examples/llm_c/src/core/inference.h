#ifndef LLM_C_CORE_INFERENCE_H
#define LLM_C_CORE_INFERENCE_H

#include "../interface/platform.h"
#include "../interface/storage.h"
#include "model.h"
#include "sampler.h"
#include "tokenizer.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Unified inference engine for GPT-2.
 *
 * This engine abstracts away platform-specific details and provides
 * a clean API for running inference on both native and IC platforms.
 */
typedef struct llm_inference_engine {
    GPT2           *model;
    GPT2Tokenizer  *tokenizer;
    llm_storage_t  *storage;
    llm_platform_t *platform;

    // Inference state
    int *sequence;
    int  seq_len;
    int  max_len;

    // Sampling parameters
    float temperature;
    int   top_k;

    // RNG state
    unsigned long long rng_state;
} llm_inference_engine_t;

/**
 * Initialize inference engine.
 *
 * @param engine Engine instance
 * @param storage Storage interface (platform-specific)
 * @param platform Platform interface (platform-specific)
 * @return 0 on success, -1 on error
 */
int llm_inference_init(llm_inference_engine_t *engine,
                       llm_storage_t          *storage,
                       llm_platform_t         *platform);

/**
 * Load model from checkpoint.
 *
 * @param engine Engine instance
 * @return 0 on success, -1 on error
 */
int llm_inference_load_model(llm_inference_engine_t *engine);

/**
 * Load model from GGUF file.
 *
 * @param engine Engine instance
 * @return 0 on success, -1 on error
 */
int llm_inference_load_model_gguf(llm_inference_engine_t *engine);

/**
 * Generate tokens from prompt.
 *
 * @param engine Engine instance
 * @param prompt_tokens Input token IDs
 * @param prompt_len Number of prompt tokens
 * @param max_new_tokens Maximum number of tokens to generate
 * @param out_tokens Output: generated token IDs (caller must free)
 * @return Number of generated tokens, or -1 on error
 */
int llm_inference_generate(llm_inference_engine_t *engine,
                           const int              *prompt_tokens,
                           int                     prompt_len,
                           int                     max_new_tokens,
                           int                   **out_tokens);

/**
 * Cleanup inference engine.
 *
 * @param engine Engine instance
 */
void llm_inference_cleanup(llm_inference_engine_t *engine);

#endif // LLM_C_CORE_INFERENCE_H
