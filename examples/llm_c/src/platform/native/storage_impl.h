#ifndef LLM_C_PLATFORM_NATIVE_STORAGE_H
#define LLM_C_PLATFORM_NATIVE_STORAGE_H

#include "../../interface/storage.h"

llm_storage_t *llm_storage_create_native(const char *checkpoint_path,
                                         const char *vocab_path,
                                         const char *merges_path);
llm_storage_t *llm_storage_create_native_with_gguf(const char *checkpoint_path,
                                                   const char *vocab_path,
                                                   const char *merges_path,
                                                   const char *gguf_path);
void           llm_storage_free_native(llm_storage_t *storage);

#endif // LLM_C_PLATFORM_NATIVE_STORAGE_H
