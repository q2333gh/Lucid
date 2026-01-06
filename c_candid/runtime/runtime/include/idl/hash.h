#ifndef IDL_HASH_H
#define IDL_HASH_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Hash a field name following Candid's specification.
 * Equivalent to the Rust `idl_hash` helper.
 */
uint32_t idl_hash(const char *text);

typedef struct idl_field_id {
    uint32_t id;
    size_t   index;
} idl_field_id;

/**
 * Sort field IDs by `id` in ascending order (stable with respect to `index`).
 */
void idl_field_id_sort(idl_field_id *fields, size_t len);

/**
 * Ensure that sorted field IDs are unique.
 * Returns IDL_STATUS_OK if unique, otherwise IDL_STATUS_ERR_INVALID_ARG.
 */
idl_status idl_field_id_verify_unique(const idl_field_id *fields, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* IDL_HASH_H */
