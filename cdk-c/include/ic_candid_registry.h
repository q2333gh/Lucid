// Candid Interface Registry for automatic .did generation
// Provides macros to register methods and generate candid interface at runtime
#pragma once

#include <stdbool.h>
#include <stddef.h>

// =============================================================================
// Method Types
// =============================================================================
typedef enum {
    IC_METHOD_QUERY,
    IC_METHOD_UPDATE,
} ic_method_type_t;

// =============================================================================
// Method Registration Entry
// =============================================================================
typedef struct {
    const char *name;      // Method name (e.g., "greet")
    const char *signature; // Candid type signature (e.g., "(text) -> (text)")
    ic_method_type_t type; // Query or Update
} ic_candid_method_t;

// =============================================================================
// Registry Configuration
// =============================================================================
#ifndef IC_CANDID_MAX_METHODS
#define IC_CANDID_MAX_METHODS 64
#endif

#ifndef IC_CANDID_DID_BUFFER_SIZE
#define IC_CANDID_DID_BUFFER_SIZE 4096
#endif

// =============================================================================
// Registry Data (defined in ic_candid_registry.c)
// =============================================================================
extern ic_candid_method_t __ic_candid_methods[];
extern int                __ic_candid_method_count;

// =============================================================================
// Registration Macros
// =============================================================================
// Use these macros to register methods with their Candid signatures.
// The signature should be in Candid format, e.g., "(text) -> (text)"
//
// Example:
//   IC_CANDID_QUERY(greet, "(text) -> (text)")
//   IC_CANDID_UPDATE(set_value, "(nat32) -> ()")

#define IC_CANDID_QUERY(func_name, sig)                                        \
    __attribute__((constructor)) static void __ic_register_##func_name(void) { \
        if (__ic_candid_method_count < IC_CANDID_MAX_METHODS) {                \
            __ic_candid_methods[__ic_candid_method_count++] =                  \
                (ic_candid_method_t){                                          \
                    .name = #func_name,                                        \
                    .signature = sig,                                          \
                    .type = IC_METHOD_QUERY,                                   \
                };                                                             \
        }                                                                      \
    }

#define IC_CANDID_UPDATE(func_name, sig)                                       \
    __attribute__((constructor)) static void __ic_register_##func_name(void) { \
        if (__ic_candid_method_count < IC_CANDID_MAX_METHODS) {                \
            __ic_candid_methods[__ic_candid_method_count++] =                  \
                (ic_candid_method_t){                                          \
                    .name = #func_name,                                        \
                    .signature = sig,                                          \
                    .type = IC_METHOD_UPDATE,                                  \
                };                                                             \
        }                                                                      \
    }

// =============================================================================
// Combined Registration + Export Macros (Recommended)
// =============================================================================
// These macros both register the method AND define the export attribute.
// Use these for a cleaner, single-point-of-declaration approach.
//
// Example:
//   IC_QUERY(greet, "(text) -> (text)") {
//       // function body
//   }
//
// This expands to both:
//   1. Registration of "greet" with signature "(text) -> (text)" as a query
//   2. Export of the function as "canister_query greet"

#define IC_QUERY(func_name, sig)                                               \
    IC_CANDID_QUERY(func_name, sig)                                            \
    __attribute__((export_name("canister_query " #func_name)))                 \
    __attribute__((visibility("default"))) void                                \
    func_name(void)

#define IC_UPDATE(func_name, sig)                                              \
    IC_CANDID_UPDATE(func_name, sig)                                           \
    __attribute__((export_name("canister_update " #func_name)))                \
    __attribute__((visibility("default"))) void                                \
    func_name(void)

// =============================================================================
// DID Generation Functions
// =============================================================================

// Generate the complete .did service interface string
// Returns a pointer to a static buffer containing the generated DID
// The buffer is valid until the next call to this function
const char *ic_candid_generate_did(void);

// Get the number of registered methods
int ic_candid_get_method_count(void);

// Get a registered method by index (0-based)
// Returns NULL if index is out of bounds
const ic_candid_method_t *ic_candid_get_method(int index);

// =============================================================================
// Automatic DID Export
// =============================================================================
// Use this macro ONCE in your main source file to automatically export
// the get_candid_pointer function for candid-extractor compatibility.
//
// Example:
//   IC_CANDID_EXPORT_DID()

#define IC_CANDID_EXPORT_DID()                                                 \
    __attribute__((export_name("get_candid_pointer")))                         \
    __attribute__((visibility("default"))) const char *                        \
    get_candid_pointer(void) {                                                 \
        return ic_candid_generate_did();                                       \
    }
