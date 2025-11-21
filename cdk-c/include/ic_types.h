// Basic type definitions for IC C SDK
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Result type for error handling
typedef enum {
    IC_OK = 0,
    IC_ERR = 1,
    IC_ERR_INVALID_ARG = 2,
    IC_ERR_OUT_OF_MEMORY = 3,
    IC_ERR_BUFFER_OVERFLOW = 4,
    IC_ERR_INVALID_STATE = 5
} ic_result_t;

// Principal type (max 29 bytes)
#define IC_PRINCIPAL_MAX_LEN 29

typedef struct {
    uint8_t bytes[IC_PRINCIPAL_MAX_LEN];
    size_t len;
} ic_principal_t;
