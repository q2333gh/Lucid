#define CDK_ALLOC_IMPLEMENTATION
#include "idl/cdk_alloc.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(__wasm32__) || defined(__wasm32_wasi__)
#define BUDDY_ALLOC_IMPLEMENTATION
#include "buddy_alloc.h"

#ifndef CDK_BUDDY_ARENA_SIZE
#define CDK_BUDDY_ARENA_SIZE (1 << 20) /* default to 1 MiB */
#endif

static unsigned char cdk_buddy_arena[CDK_BUDDY_ARENA_SIZE];
static struct buddy *cdk_buddy_handle = NULL;

static struct buddy *cdk_buddy_instance(void) {
    if (cdk_buddy_handle != NULL) {
        return cdk_buddy_handle;
    }
    cdk_buddy_handle = buddy_embed(cdk_buddy_arena, CDK_BUDDY_ARENA_SIZE);
    return cdk_buddy_handle;
}
#endif

static bool cdk_mul_overflow(size_t a, size_t b, size_t *result) {
    if (b == 0) {
        *result = 0;
        return false;
    }
    if (a > SIZE_MAX / b) {
        return true;
    }
    *result = a * b;
    return false;
}

void *cdk_malloc(size_t size) {
#if defined(__wasm32__) || defined(__wasm32_wasi__)
    struct buddy *buddy = cdk_buddy_instance();
    if (!buddy) {
        return NULL;
    }
    if (size == 0) {
        size = 1;
    }
    return buddy_malloc(buddy, size);
#else
    return malloc(size);
#endif
}

void *cdk_calloc(size_t nmemb, size_t size) {
#if defined(__wasm32__) || defined(__wasm32_wasi__)
    struct buddy *buddy = cdk_buddy_instance();
    if (!buddy) {
        return NULL;
    }
    return buddy_calloc(buddy, nmemb, size);
#else
    return calloc(nmemb, size);
#endif
}

void *cdk_realloc(void *ptr, size_t size) {
#if defined(__wasm32__) || defined(__wasm32_wasi__)
    struct buddy *buddy = cdk_buddy_instance();
    if (!buddy) {
        return NULL;
    }
    if (size == 0) {
        if (ptr != NULL) {
            buddy_free(buddy, ptr);
        }
        return NULL;
    }
    if (ptr == NULL) {
        return buddy_malloc(buddy, size);
    }
    return buddy_realloc(buddy, ptr, size, false);
#else
    return realloc(ptr, size);
#endif
}

void *cdk_reallocarray(void *ptr, size_t nmemb, size_t size) {
    size_t total;
    if (cdk_mul_overflow(nmemb, size, &total)) {
        return NULL;
    }
    return cdk_realloc(ptr, total);
}

void cdk_free(void *ptr) {
#if defined(__wasm32__) || defined(__wasm32_wasi__)
    if (ptr == NULL) {
        return;
    }
    struct buddy *buddy = cdk_buddy_instance();
    if (!buddy) {
        return;
    }
    buddy_free(buddy, ptr);
#else
    free(ptr);
#endif
}
