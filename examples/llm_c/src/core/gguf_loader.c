/*
 * GGUF Loader - Platform Independent
 *
 * Loads GPT-2 models from GGUF format files.
 */

#include "gguf_loader.h"
#include "fp16.h"
#include "gguflib.h"
#include "model.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static gguf_ctx *gguf_open_auto(const uint8_t *data, size_t len) {
    if (gguf_is_valid(data, len)) {
        return gguf_open_from_memory(data, len);
    }
    return gguf_open((const char *)data);
}

int gguf_is_valid(const uint8_t *data, size_t len) {
    if (!data || len < 8)
        return 0;

    // Check GGUF magic number: 0x47 0x47 0x55 0x46 ("GGUF")
    const uint8_t *magic = data;
    if (magic[0] == 0x47 && magic[1] == 0x47 && magic[2] == 0x55 &&
        magic[3] == 0x46) {
        return 1;
    }
    return 0;
}

int gguf_parse_config(const uint8_t *data, size_t len, GPT2Config *config) {
    if (!data || !config) {
        return -1;
    }

    // For now, we require file-based API
    // This function will be called with file path in native platform
    // For IC platform, we'll need memory-based parsing (future work)

    // Parse from file path (data points to filename string)
    gguf_ctx *ctx = gguf_open_auto(data, len);
    if (!ctx) {
        return -1;
    }

    // Extract model configuration from metadata
    gguf_key key;
    memset(config, 0, sizeof(GPT2Config));

    // Default values for tiny-gpt2
    config->max_seq_len = 1024;
    config->vocab_size = 50257;
    config->num_layers = 12;
    config->num_heads = 12;
    config->channels = 768;

    // Parse metadata keys
    while (gguf_get_key(ctx, &key)) {
        if (key.type == GGUF_VALUE_TYPE_UINT32) {
            if (strncmp(key.name, "gpt2.context_length", key.namelen) == 0) {
                config->max_seq_len = key.val->uint32;
            } else if (strncmp(key.name, "gpt2.embedding_length",
                               key.namelen) == 0) {
                config->channels = key.val->uint32;
            } else if (strncmp(key.name, "gpt2.block_count", key.namelen) ==
                       0) {
                config->num_layers = key.val->uint32;
            } else if (strncmp(key.name, "gpt2.attention.head_count",
                               key.namelen) == 0) {
                config->num_heads = key.val->uint32;
            }
        }

        // Skip value data
        gguf_do_with_value(ctx, key.type, key.val, NULL, 0, 0, NULL);
    }

    gguf_close(ctx);
    return 0;
}

int gguf_load_weights(const uint8_t *data,
                      size_t         len,
                      GPT2          *model,
                      void *(*alloc_fn)(size_t)) {
    if (!data || !model || !alloc_fn)
        return -1;

    gguf_ctx *ctx = gguf_open_auto(data, len);
    if (!ctx)
        return -1;

    // Skip metadata section
    gguf_skip_key_values_section(ctx);

    // Map tensor names to GPT2 parameter locations
    // GPT-2 tensor naming convention in GGUF:
    // - token_embd.weight -> wte
    // - pos_embd.weight -> wpe
    // - blk.{i}.attn_norm.weight -> ln1w[i]
    // - blk.{i}.attn_norm.bias -> ln1b[i]
    // - blk.{i}.attn_q.weight -> qkvw[i] (q part)
    // - blk.{i}.attn_k.weight -> qkvw[i] (k part)
    // - blk.{i}.attn_v.weight -> qkvw[i] (v part)
    // etc.

    gguf_tensor tensor;
    int         layer_idx = -1;

    // First pass: count layers and allocate memory
    gguf_rewind(ctx);
    gguf_skip_key_values_section(ctx);

    int max_layer = -1;
    while (gguf_get_tensor(ctx, &tensor)) {
        if (tensor.name) {
            // Extract layer index from tensor name
            const char *blk_pos = strstr(tensor.name, "blk.");
            if (blk_pos) {
                int layer = atoi(blk_pos + 4);
                if (layer > max_layer)
                    max_layer = layer;
            }
        }
    }

    if (max_layer < 0) {
        // Try alternative naming
        max_layer = model->config.num_layers - 1;
    }

    // Prepare parameter layout from config
    // Create model_header array: [magic, version, maxT, V, L, NH, C]
    int model_header[7] = {0x67676d66,
                           1,
                           model->config.max_seq_len,
                           model->config.vocab_size,
                           model->config.num_layers,
                           model->config.num_heads,
                           model->config.channels};
    gpt2_prepare_param_layout(model, model_header);

    // Second pass: load weights
    gguf_rewind(ctx);
    gguf_skip_key_values_section(ctx);

    while (gguf_get_tensor(ctx, &tensor)) {
        if (!tensor.name)
            break;

        // Convert tensor to float32
        float *weights = gguf_tensor_to_float(&tensor);
        if (!weights) {
            // Try F16 conversion
            int16_t *f16_weights = gguf_tensor_to_f16(&tensor);
            if (f16_weights) {
                // Convert F16 to F32
                size_t num_elements = tensor.num_weights;
                weights = (float *)alloc_fn(num_elements * sizeof(float));
                if (weights) {
                    for (size_t i = 0; i < num_elements; i++) {
                        weights[i] = from_half((uint16_t)f16_weights[i]);
                    }
                }
            }
        }

        if (!weights)
            continue;

        // Map tensor to GPT2 parameter based on name
        // This is a simplified mapping - full implementation would
        // handle all GPT-2 parameter names correctly
        const char *name = tensor.name;

        if (strstr(name, "token_embd.weight")) {
            // wte: (V, C)
            if (tensor.ndim == 2 && tensor.dim[1] == model->config.channels) {
                size_t rows = tensor.dim[0];
                size_t max_rows = (size_t)model->config.vocab_size;
                size_t copy_rows = rows < max_rows ? rows : max_rows;
                if (copy_rows > 0) {
                    memcpy(model->params.wte, weights,
                           copy_rows * (size_t)model->config.channels *
                               sizeof(float));
                }
            }
        } else if (strstr(name, "pos_embd.weight")) {
            // wpe: (maxT, C)
            if (tensor.ndim == 2 && tensor.dim[1] == model->config.channels) {
                size_t rows = tensor.dim[0];
                size_t max_rows = (size_t)model->config.max_seq_len;
                size_t copy_rows = rows < max_rows ? rows : max_rows;
                if (copy_rows > 0) {
                    memcpy(model->params.wpe, weights,
                           copy_rows * (size_t)model->config.channels *
                               sizeof(float));
                }
            }
        }
        // TODO: Map remaining tensors (layer weights, etc.)

        // Note: weights may be owned by the gguf library or allocated here;
        // we don't have a matching free_fn, so avoid freeing via alloc_fn(0).
    }

    gguf_close(ctx);
    return 0;
}

int gguf_extract_tokenizer(const uint8_t *data,
                           size_t         len,
                           uint8_t      **vocab_out,
                           size_t        *vocab_len,
                           uint8_t      **merges_out,
                           size_t        *merges_len,
                           void *(*alloc_fn)(size_t)) {
    if (!data || !alloc_fn)
        return -1;

    gguf_ctx *ctx = gguf_open_auto(data, len);
    if (!ctx)
        return -1;

    *vocab_out = NULL;
    *vocab_len = 0;
    *merges_out = NULL;
    *merges_len = 0;

    gguf_key key;
    while (gguf_get_key(ctx, &key)) {
        if (key.type == GGUF_VALUE_TYPE_STRING) {
            if (strncmp(key.name, "tokenizer.ggml.vocab", key.namelen) == 0 ||
                strncmp(key.name, "tokenizer.ggml.tokens", key.namelen) == 0) {
                // Extract vocab JSON
                size_t vocab_size = key.val->string.len;
                *vocab_out = (uint8_t *)alloc_fn(vocab_size);
                if (*vocab_out) {
                    memcpy(*vocab_out, key.val->string.string, vocab_size);
                    *vocab_len = vocab_size;
                }
            } else if (strncmp(key.name, "tokenizer.ggml.merges",
                               key.namelen) == 0) {
                // Extract merges BPE
                size_t merges_size = key.val->string.len;
                *merges_out = (uint8_t *)alloc_fn(merges_size);
                if (*merges_out) {
                    memcpy(*merges_out, key.val->string.string, merges_size);
                    *merges_len = merges_size;
                }
            }
        }

        gguf_do_with_value(ctx, key.type, key.val, NULL, 0, 0, NULL);
    }

    gguf_close(ctx);

    if (!*vocab_out || !*merges_out)
        return -1;

    return 0;
}
