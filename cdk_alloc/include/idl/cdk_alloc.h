#ifndef CDK_ALLOC_H
#define CDK_ALLOC_H

#include <stddef.h>


void *cdk_malloc(size_t size);
void *cdk_calloc(size_t nmemb, size_t size);
void *cdk_realloc(void *ptr, size_t size);
void *cdk_reallocarray(void *ptr, size_t nmemb, size_t size);
void  cdk_free(void *ptr);

#if (defined(__wasm32__) || defined(__wasm32_wasi__)) &&                       \
    !defined(CDK_ALLOC_IMPLEMENTATION)
/* Redirect standard memory functions to CDK implementation for WASM builds */
#define malloc cdk_malloc
#define calloc cdk_calloc
#define realloc cdk_realloc
#define reallocarray cdk_reallocarray
#define free cdk_free
#endif

#endif /* CDK_ALLOC_H */
