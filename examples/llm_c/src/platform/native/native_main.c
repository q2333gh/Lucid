/*
 * Native Main Entry Point
 *
 * Replaces run_gpt2.c with platform-abstracted version.
 */

#include "../../core/inference.h"
#include "../../core/tokenizer.h"
#include "../../platform/native/platform_impl.h"
#include "../../platform/native/storage_impl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GPT2_EOT 50256

int main(int argc, char **argv) {
    const char *checkpoint_path = "assets/gpt2_124M.bin";
    const char *vocab_path = "assets/encoder.json";
    const char *merges_path = "assets/vocab.bpe";
    const char *gguf_path = NULL;
    const char *prompt = "Hello";
    int         max_new_tokens = 32;
    float       temperature = 1.0f;
    int         top_k = 40;
    int         use_gguf = 0;

    // Parse arguments (simplified)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--checkpoint") == 0 && i + 1 < argc) {
            checkpoint_path = argv[++i];
        } else if (strcmp(argv[i], "--vocab") == 0 && i + 1 < argc) {
            vocab_path = argv[++i];
        } else if (strcmp(argv[i], "--merges") == 0 && i + 1 < argc) {
            merges_path = argv[++i];
        } else if (strcmp(argv[i], "--gguf") == 0 && i + 1 < argc) {
            gguf_path = argv[++i];
            use_gguf = 1;
        } else if (strcmp(argv[i], "--prompt") == 0 && i + 1 < argc) {
            prompt = argv[++i];
        } else if (strcmp(argv[i], "--max-new") == 0 && i + 1 < argc) {
            max_new_tokens = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--temp") == 0 && i + 1 < argc) {
            temperature = atof(argv[++i]);
        } else if (strcmp(argv[i], "--top-k") == 0 && i + 1 < argc) {
            top_k = atoi(argv[++i]);
        }
    }

    // Create platform adapters
    fprintf(stderr, "Creating storage (use_gguf=%d, path=%s)\n", use_gguf,
            gguf_path ? gguf_path : "NULL");
    llm_storage_t *storage = NULL;
    if (use_gguf) {
        fprintf(stderr, "Using GGUF storage\n");
        storage = llm_storage_create_native_with_gguf(
            checkpoint_path, vocab_path, merges_path, gguf_path);
    } else {
        fprintf(stderr, "Using regular storage\n");
        storage =
            llm_storage_create_native(checkpoint_path, vocab_path, merges_path);
    }

    if (!storage) {
        fprintf(stderr, "Failed to create storage\n");
        return 1;
    }
    fprintf(stderr, "Storage created successfully\n");

    llm_platform_t *platform = llm_platform_create_native();
    if (!platform) {
        fprintf(stderr, "Failed to create platform\n");
        llm_storage_free_native(storage);
        return 1;
    }

    // Initialize inference engine
    fprintf(stderr, "Initializing inference engine\n");
    llm_inference_engine_t engine;
    if (llm_inference_init(&engine, storage, platform) != 0) {
        fprintf(stderr, "Failed to initialize inference engine\n");
        llm_platform_free_native(platform);
        llm_storage_free_native(storage);
        return 1;
    }
    fprintf(stderr, "Inference engine initialized\n");

    engine.temperature = temperature;
    engine.top_k = top_k;

    // Load model
    int load_result = 0;
    if (use_gguf) {
        fprintf(stderr, "Loading GGUF model from: %s\n", gguf_path);
        load_result = llm_inference_load_model_gguf(&engine);
        if (load_result != 0) {
            fprintf(stderr, "Failed to load GGUF model (error code: %d)\n",
                    load_result);
        }
    } else {
        load_result = llm_inference_load_model(&engine);
        if (load_result != 0) {
            fprintf(stderr, "Failed to load model (error code: %d)\n",
                    load_result);
        }
    }

    if (load_result != 0) {
        fprintf(stderr, "Failed to load model\n");
        llm_inference_cleanup(&engine);
        llm_platform_free_native(platform);
        llm_storage_free_native(storage);
        return 1;
    }

    fprintf(stderr, "Model loaded successfully\n");
    fprintf(stderr, "Config: channels=%d, layers=%d, heads=%d, vocab=%d\n",
            engine.model->config.channels, engine.model->config.num_layers,
            engine.model->config.num_heads, engine.model->config.vocab_size);

    // Encode prompt
    int  prompt_len = 0;
    int *prompt_tokens = gpt2_tokenizer_encode(engine.tokenizer, prompt,
                                               &prompt_len, platform->alloc);
    if (!prompt_tokens) {
        fprintf(stderr, "Failed to encode prompt\n");
        llm_inference_cleanup(&engine);
        llm_platform_free_native(platform);
        llm_storage_free_native(storage);
        return 1;
    }

    // Generate
    int *output_tokens = NULL;
    int  num_generated = llm_inference_generate(
         &engine, prompt_tokens, prompt_len, max_new_tokens, &output_tokens);

    if (num_generated < 0) {
        fprintf(stderr, "Failed to generate tokens\n");
        platform->free(prompt_tokens);
        llm_inference_cleanup(&engine);
        llm_platform_free_native(platform);
        llm_storage_free_native(storage);
        return 1;
    }

    // Decode and print
    char *decoded =
        gpt2_tokenizer_decode(engine.tokenizer, output_tokens,
                              prompt_len + num_generated, platform->alloc);
    if (decoded) {
        printf("%s", decoded);
        platform->free(decoded);
    }
    printf("\n");

    // Cleanup
    platform->free(prompt_tokens);
    platform->free(output_tokens);
    llm_inference_cleanup(&engine);
    llm_platform_free_native(platform);
    llm_storage_free_native(storage);

    return 0;
}
