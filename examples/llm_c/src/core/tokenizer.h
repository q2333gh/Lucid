#ifndef LLM_C_CORE_TOKENIZER_H
#define LLM_C_CORE_TOKENIZER_H

#include <stddef.h>

/**
 * GPT-2 Tokenizer (platform-independent core)
 *
 * This tokenizer uses BPE (Byte Pair Encoding) algorithm.
 * All I/O operations are abstracted through storage interface.
 */
typedef struct GPT2Tokenizer GPT2Tokenizer;

/**
 * Load tokenizer from memory buffers (platform-independent).
 *
 * @param vocab_data JSON vocab data (encoder.json content)
 * @param vocab_len Length of vocab_data
 * @param merges_data BPE merges data (vocab.bpe content)
 * @param merges_len Length of merges_data
 * @param alloc_fn Memory allocation function
 * @return Tokenizer instance or NULL on error
 */
GPT2Tokenizer *gpt2_tokenizer_load_from_memory(const char *vocab_data,
                                               size_t      vocab_len,
                                               const char *merges_data,
                                               size_t      merges_len,
                                               void *(*alloc_fn)(size_t));

/**
 * Free tokenizer instance.
 *
 * @param tok Tokenizer instance
 * @param free_fn Memory free function
 */
void gpt2_tokenizer_free(GPT2Tokenizer *tok, void (*free_fn)(void *));

/**
 * Encode text to token IDs.
 *
 * @param tok Tokenizer instance
 * @param text Input text
 * @param out_len Output: number of tokens
 * @param alloc_fn Memory allocation function
 * @return Array of token IDs (caller must free)
 */
int *gpt2_tokenizer_encode(GPT2Tokenizer *tok,
                           const char    *text,
                           int           *out_len,
                           void *(*alloc_fn)(size_t));

/**
 * Decode token IDs to text.
 *
 * @param tok Tokenizer instance
 * @param token_ids Array of token IDs
 * @param num_tokens Number of tokens
 * @param alloc_fn Memory allocation function
 * @return Decoded text string (caller must free)
 */
char *gpt2_tokenizer_decode(GPT2Tokenizer *tok,
                            const int     *token_ids,
                            int            num_tokens,
                            void *(*alloc_fn)(size_t));

#endif // LLM_C_CORE_TOKENIZER_H
