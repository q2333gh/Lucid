#ifndef LLM_C_PLATFORM_H
#define LLM_C_PLATFORM_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Platform abstraction interface for platform-independent operations.
 *
 * This interface allows the core inference engine to use platform services
 * (memory, I/O, error handling) without knowing the underlying platform.
 */
typedef struct llm_platform {
    /**
     * Allocate memory.
     * Native: malloc
     * IC: cdk_alloc or malloc (via buddy allocator)
     */
    void *(*alloc)(size_t size);

    /**
     * Free memory.
     * Native: free
     * IC: free or cdk_alloc free
     */
    void (*free)(void *ptr);

    /**
     * Print debug message (no formatting).
     * Native: printf
     * IC: ic_api_debug_print
     */
    void (*debug_print)(const char *msg);

    /**
     * Print formatted debug message.
     * Native: printf
     * IC: ic_api_debug_printf (via tinyprintf)
     */
    void (*debug_printf)(const char *fmt, ...);

    /**
     * Handle fatal error and exit.
     * Native: exit(1)
     * IC: ic_api_trap
     */
    void (*error_exit)(const char *msg);

    /**
     * Get random seed for RNG initialization.
     * Native: time(NULL)
     * IC: canister time or caller principal hash
     */
    unsigned long long (*get_random_seed)(void);
} llm_platform_t;

#endif // LLM_C_PLATFORM_H
