// Candid Interface Registry Implementation
// Generates .did service interface from registered methods

#include "ic_candid_registry.h"

// =============================================================================
// Registry Storage
// =============================================================================
ic_candid_method_t __ic_candid_methods[IC_CANDID_MAX_METHODS];
int                __ic_candid_method_count = 0;

// Static buffer for generated DID string
static char __ic_did_buffer[IC_CANDID_DID_BUFFER_SIZE];

// =============================================================================
// Helper: copy string and return pointer to end
// =============================================================================
static char *str_copy(char *dst, char *end, const char *src) {
    while (*src && dst < end) {
        *dst++ = *src++;
    }
    return dst;
}

// =============================================================================
// DID Generation
// =============================================================================

const char *ic_candid_generate_did(void) {
    char *p = __ic_did_buffer;
    char *end = __ic_did_buffer + IC_CANDID_DID_BUFFER_SIZE - 1;

    // Write service header
    p = str_copy(p, end, "service : {\n");

    // Write each registered method
    for (int i = 0; i < __ic_candid_method_count && p < end; i++) {
        const ic_candid_method_t *method = &__ic_candid_methods[i];

        // Format: "    method_name : signature annotation;\n"
        p = str_copy(p, end, "    ");
        p = str_copy(p, end, method->name);
        p = str_copy(p, end, " : ");
        p = str_copy(p, end, method->signature);

        // Add query annotation if needed
        if (method->type == IC_METHOD_QUERY) {
            p = str_copy(p, end, " query");
        }

        p = str_copy(p, end, ";\n");
    }

    // Write closing brace
    if (p < end) {
        *p++ = '}';
    }
    *p = '\0';

    return __ic_did_buffer;
}

int ic_candid_get_method_count(void) { return __ic_candid_method_count; }

const ic_candid_method_t *ic_candid_get_method(int index) {
    if (index < 0 || index >= __ic_candid_method_count) {
        return NULL;
    }
    return &__ic_candid_methods[index];
}
