/*
 * IC Platform Implementation
 *
 * Implements platform interface using IC C SDK.
 */

#include "../../interface/platform.h"
#include "ic_c_sdk.h"
#include "tinyprintf.h"
#include <stdarg.h>
#include <stdlib.h>

static void *ic_alloc(size_t size) {
    return malloc(size); // Use malloc for now, can switch to cdk_alloc later
}

static void ic_free(void *ptr) { free(ptr); }

static void ic_debug_print(const char *msg) { ic_api_debug_print(msg); }

static void ic_debug_printf(const char *fmt, ...) {
    char    buf[512];
    va_list args;
    va_start(args, fmt);
    tfp_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ic_api_debug_print(buf);
}

static void ic_error_exit(const char *msg) { ic_api_trap(msg); }

static unsigned long long ic_get_random_seed(void) {
    // Use canister time or caller principal hash
    return (unsigned long long)ic_api_time();
}

llm_platform_t *llm_platform_create_ic(void) {
    llm_platform_t *platform = (llm_platform_t *)malloc(sizeof(llm_platform_t));
    if (!platform) {
        return NULL;
    }

    platform->alloc = ic_alloc;
    platform->free = ic_free;
    platform->debug_print = ic_debug_print;
    platform->debug_printf = ic_debug_printf;
    platform->error_exit = ic_error_exit;
    platform->get_random_seed = ic_get_random_seed;

    return platform;
}

void llm_platform_free_ic(llm_platform_t *platform) { free(platform); }
