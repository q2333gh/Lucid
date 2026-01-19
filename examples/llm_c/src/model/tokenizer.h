#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stddef.h>

typedef struct GPT2Tokenizer GPT2Tokenizer;

GPT2Tokenizer *tokenizer_load(const char *vocab_path, const char *merges_path);
GPT2Tokenizer *tokenizer_load_from_memory(const char *vocab_data,
                                          size_t      vocab_len,
                                          const char *merges_data,
                                          size_t      merges_len);
void           tokenizer_free(GPT2Tokenizer *tok);

int *tokenizer_encode(GPT2Tokenizer *tok, const char *text, int *out_len);
char *
tokenizer_decode(GPT2Tokenizer *tok, const int *token_ids, int num_tokens);

#endif
