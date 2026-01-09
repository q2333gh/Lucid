/*
 * Base Utilities
 *
 * Provides basic utility functions for the Candid runtime:
 * - Status code to string conversion for error messages
 * - Common error handling utilities
 *
 * This module contains foundational utilities used throughout the Candid
 * implementation for consistent error reporting and status handling.
 */
#include "idl/base.h"

#include <stddef.h>

const char *idl_status_message(idl_status status) {
    switch (status) {
    case IDL_STATUS_OK:
        return "ok";
    case IDL_STATUS_ERR_OVERFLOW:
        return "overflow";
    case IDL_STATUS_ERR_TRUNCATED:
        return "truncated input";
    case IDL_STATUS_ERR_ALLOC:
        return "allocation failure";
    case IDL_STATUS_ERR_INVALID_ARG:
        return "invalid argument";
    case IDL_STATUS_ERR_UNSUPPORTED:
        return "unsupported operation";
    }
    return "unknown status";
}
