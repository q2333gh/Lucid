#include "idl/hash.h"

#include <stddef.h>

uint32_t idl_hash(const char *text) {
    uint32_t hash = 0;
    if (text == NULL) {
        return hash;
    }
    while (*text) {
        hash = hash * 223u + (unsigned char)(*text);
        ++text;
    }
    return hash;
}

void idl_field_id_sort(idl_field_id *fields, size_t len) {
    if (fields == NULL || len < 2) {
        return;
    }
    // Stable insertion sort (len is typically small for Candid records).
    for (size_t i = 1; i < len; ++i) {
        idl_field_id key = fields[i];
        size_t       j = i;
        while (j > 0 && fields[j - 1].id > key.id) {
            fields[j] = fields[j - 1];
            --j;
        }
        fields[j] = key;
    }
}

idl_status idl_field_id_verify_unique(const idl_field_id *fields, size_t len) {
    if (fields == NULL && len > 0) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    for (size_t i = 1; i < len; ++i) {
        if (fields[i - 1].id == fields[i].id) {
            return IDL_STATUS_ERR_INVALID_ARG;
        }
    }
    return IDL_STATUS_OK;
}
