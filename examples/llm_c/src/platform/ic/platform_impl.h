#ifndef LLM_C_PLATFORM_IC_PLATFORM_H
#define LLM_C_PLATFORM_IC_PLATFORM_H

#include "../../interface/platform.h"

llm_platform_t *llm_platform_create_ic(void);
void            llm_platform_free_ic(llm_platform_t *platform);

#endif // LLM_C_PLATFORM_IC_PLATFORM_H
