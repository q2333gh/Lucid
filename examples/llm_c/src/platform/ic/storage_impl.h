#ifndef LLM_C_PLATFORM_IC_STORAGE_H
#define LLM_C_PLATFORM_IC_STORAGE_H

#include "../../interface/storage.h"

llm_storage_t *llm_storage_create_ic(void);
void           llm_storage_free_ic(llm_storage_t *storage);

#endif // LLM_C_PLATFORM_IC_STORAGE_H
