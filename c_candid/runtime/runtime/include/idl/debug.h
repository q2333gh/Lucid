/**
 * Debug printing utilities for c_candid
 * Only enabled on non-WASM platforms via CANDID_DEBUG_PRINT_ENABLED
 */
#ifndef IDL_DEBUG_H
#define IDL_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CANDID_DEBUG_PRINT_ENABLED
#include <stdio.h>

// Debug print macros - only active when CANDID_DEBUG_PRINT_ENABLED is defined
#define IDL_DEBUG_PRINT(fmt, ...)                                              \
    fprintf(stderr, "[c_candid] " fmt "\n", ##__VA_ARGS__)

#define IDL_DEBUG_PRINTF(fmt, ...)                                             \
    fprintf(stderr, "[c_candid] " fmt, ##__VA_ARGS__)

#define IDL_DEBUG_HEX(label, data, len)                                        \
    do {                                                                       \
        fprintf(stderr, "[c_candid] %s: ", label);                             \
        for (size_t _i = 0; _i < (len); _i++) {                                \
            fprintf(stderr, "%02x", ((const unsigned char *)(data))[_i]);      \
        }                                                                      \
        fprintf(stderr, "\n");                                                 \
    } while (0)

#else
// No-op macros when debugging is disabled (e.g., WASM builds)
#define IDL_DEBUG_PRINT(fmt, ...) ((void)0)
#define IDL_DEBUG_PRINTF(fmt, ...) ((void)0)
#define IDL_DEBUG_HEX(label, data, len) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif // IDL_DEBUG_H
