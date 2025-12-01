#ifndef IDL_BASE_H
#define IDL_BASE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum idl_status {
    IDL_STATUS_OK = 0,
    IDL_STATUS_ERR_OVERFLOW,
    IDL_STATUS_ERR_TRUNCATED,
    IDL_STATUS_ERR_ALLOC,
    IDL_STATUS_ERR_INVALID_ARG,
    IDL_STATUS_ERR_UNSUPPORTED,
} idl_status;

const char *idl_status_message(idl_status status);

#ifdef __cplusplus
}
#endif

#endif /* IDL_BASE_H */
