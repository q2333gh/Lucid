/*
 * Native Platform Implementation
 *
 * Implements platform interface using native stdlib functions.
 */

#include "../../interface/platform.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void *native_alloc(size_t size) { return malloc(size); }

static void native_free(void *ptr) { free(ptr); }

static void native_debug_print(const char *msg) { printf("%s", msg); }

static void native_debug_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static void native_error_exit(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

static unsigned long long native_get_random_seed(void) {
    return (unsigned long long)time(NULL);
}

llm_platform_t *llm_platform_create_native(void) {
    llm_platform_t *platform = (llm_platform_t *)malloc(sizeof(llm_platform_t));
    if (!platform) {
        return NULL;
    }

    platform->alloc = native_alloc;
    platform->free = native_free;
    platform->debug_print = native_debug_print;
    platform->debug_printf = native_debug_printf;
    platform->error_exit = native_error_exit;
    platform->get_random_seed = native_get_random_seed;

    return platform;
}

void llm_platform_free_native(llm_platform_t *platform) { free(platform); }
