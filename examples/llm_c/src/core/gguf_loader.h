#ifndef LLM_C_GGUF_LOADER_H
#define LLM_C_GGUF_LOADER_H

#include "model.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Parse GGUF file and extract GPT2Config.
 *
 * @param data GGUF file data (must remain valid during parsing)
 * @param len Length of data in bytes
 * @param config Output GPT2Config structure
 * @return 0 on success, -1 on error
 */
int gguf_parse_config(const uint8_t *data, size_t len, GPT2Config *config);

/**
 * Load model weights from GGUF into GPT2 structure.
 *
 * @param data GGUF file data
 * @param len Length of data in bytes
 * @param model GPT2 model structure (must be initialized)
 * @param alloc_fn Memory allocation function
 * @return 0 on success, -1 on error
 */
int gguf_load_weights(const uint8_t *data,
                      size_t         len,
                      GPT2          *model,
                      void *(*alloc_fn)(size_t));

/**
 * Extract tokenizer data (vocab, merges) from GGUF metadata.
 *
 * @param data GGUF file data
 * @param len Length of data in bytes
 * @param vocab_out Output vocab JSON data (caller must free)
 * @param vocab_len Output vocab length
 * @param merges_out Output merges BPE data (caller must free)
 * @param merges_len Output merges length
 * @param alloc_fn Memory allocation function
 * @return 0 on success, -1 on error
 */
int gguf_extract_tokenizer(const uint8_t *data,
                           size_t         len,
                           uint8_t      **vocab_out,
                           size_t        *vocab_len,
                           uint8_t      **merges_out,
                           size_t        *merges_len,
                           void *(*alloc_fn)(size_t));

/**
 * Check if data is a valid GGUF file.
 *
 * @param data File data
 * @param len Length of data
 * @return 1 if valid GGUF, 0 otherwise
 */
int gguf_is_valid(const uint8_t *data, size_t len);

#endif // LLM_C_GGUF_LOADER_H
