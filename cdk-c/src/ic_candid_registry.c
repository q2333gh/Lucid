// Candid Interface Registry Implementation
// Generates .did service interface from registered methods

#include "ic_candid_registry.h"

#include <stdio.h>
#include <string.h>

// =============================================================================
// Registry Storage
// =============================================================================
ic_candid_method_t __ic_candid_methods[IC_CANDID_MAX_METHODS];
int                __ic_candid_method_count = 0;

// Static buffer for generated DID string
static char __ic_did_buffer[IC_CANDID_DID_BUFFER_SIZE];

// =============================================================================
// DID Generation
// =============================================================================

const char *ic_candid_generate_did(void) {
    char  *p         = __ic_did_buffer;
    char  *end       = __ic_did_buffer + IC_CANDID_DID_BUFFER_SIZE - 1;
    size_t remaining = IC_CANDID_DID_BUFFER_SIZE - 1;

    // Write service header
    int written = snprintf(p, remaining, "service : {\n");
    if (written < 0 || (size_t)written >= remaining) {
        return "service : {}";
    }
    p += written;
    remaining -= written;

    // Write each registered method
    for (int i = 0; i < __ic_candid_method_count && remaining > 0; i++) {
        const ic_candid_method_t *method = &__ic_candid_methods[i];

        // Determine method annotation
        const char *annotation = "";
        if (method->type == IC_METHOD_QUERY) {
            annotation = " query";
        }
        // UPDATE methods have no annotation in Candid

        // Format: "  method_name : signature annotation;\n"
        written = snprintf(p, remaining, "    %s : %s%s;\n", method->name, method->signature,
                           annotation);

        if (written < 0 || (size_t)written >= remaining) {
            break;
        }
        p += written;
        remaining -= written;
    }

    // Write closing brace
    if (remaining > 1) {
        *p++ = '}';
        *p   = '\0';
    }

    return __ic_did_buffer;
}

int ic_candid_get_method_count(void) {
    return __ic_candid_method_count;
}

const ic_candid_method_t *ic_candid_get_method(int index) {
    if (index < 0 || index >= __ic_candid_method_count) {
        return NULL;
    }
    return &__ic_candid_methods[index];
}

