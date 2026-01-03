// Simplified argument parsing API for IC canisters
// Provides automatic memory management and easy-to-use parsing macros

#pragma once

#include "ic_api.h"
#include <string.h>

// Forward declaration
struct idl_deserializer;
typedef struct idl_deserializer idl_deserializer;

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Internal: Argument parser state
// =============================================================================
typedef struct ic_args_parser {
    ic_api_t         *api;
    idl_deserializer *deserializer;
    bool              initialized;
} ic_args_parser_t;

// =============================================================================
// Initialize argument parser (called automatically by macros)
// =============================================================================
ic_result_t ic_args_parser_init(ic_args_parser_t *parser, ic_api_t *api);

// =============================================================================
// Parse individual arguments
// =============================================================================
ic_result_t ic_args_parse_text(ic_args_parser_t *parser, char **text);
ic_result_t ic_args_parse_nat(ic_args_parser_t *parser, uint64_t *value);
ic_result_t ic_args_parse_int(ic_args_parser_t *parser, int64_t *value);
ic_result_t ic_args_parse_bool(ic_args_parser_t *parser, bool *value);
ic_result_t
ic_args_parse_blob(ic_args_parser_t *parser, uint8_t **blob, size_t *blob_len);
ic_result_t ic_args_parse_principal(ic_args_parser_t *parser,
                                    ic_principal_t   *principal);

#ifdef __cplusplus
}
#endif

// =============================================================================
// Convenience Macros for Argument Parsing
// =============================================================================

// Simple parsing for single argument functions
// Usage:
//   char *name;
//   IC_API_PARSE_SIMPLE(api, text, name);
//
//   uint64_t age;
//   IC_API_PARSE_SIMPLE(api, nat, age);
//
// Note: The type parameter is used to select the appropriate parser function
#define IC_API_PARSE_SIMPLE(api, type, var)                                    \
    do {                                                                       \
        ic_args_parser_t _parser;                                              \
        if (ic_args_parser_init(&_parser, api) == IC_OK) {                     \
            ic_result_t _res = IC_ERR_INVALID_ARG;                             \
            const char *_type_str = #type;                                     \
            if (strcmp(_type_str, "text") == 0) {                              \
                _res = ic_args_parse_text(&_parser, (char **)&(var));          \
            } else if (strcmp(_type_str, "nat") == 0) {                        \
                _res = ic_args_parse_nat(&_parser, (uint64_t *)&(var));        \
            } else if (strcmp(_type_str, "int") == 0) {                        \
                _res = ic_args_parse_int(&_parser, (int64_t *)&(var));         \
            } else if (strcmp(_type_str, "bool") == 0) {                       \
                _res = ic_args_parse_bool(&_parser, (bool *)&(var));           \
            }                                                                  \
            if (_res != IC_OK) {                                               \
                ic_api_trap("Failed to parse argument");                       \
            }                                                                  \
        } else {                                                               \
            ic_api_trap("Failed to initialize argument parser");               \
        }                                                                      \
    } while (0)

// Multi-argument parsing
// Usage:
//   char *name;
//   uint64_t age;
//   bool active;
//   IC_API_PARSE_BEGIN(api);
//   IC_ARG_TEXT(&name);
//   IC_ARG_NAT(&age);
//   IC_ARG_BOOL(&active);
//   IC_API_PARSE_END();
#define IC_API_PARSE_BEGIN(api)                                                \
    do {                                                                       \
        ic_args_parser_t _parser;                                              \
        if (ic_args_parser_init(&_parser, api) == IC_OK) {                     \
        (void)0

#define IC_API_PARSE_END()                                                     \
    }                                                                          \
    else {                                                                     \
        ic_api_trap("Failed to initialize argument parser");                   \
    }                                                                          \
    }                                                                          \
    while (0)

// Argument parsing macros (used between IC_API_PARSE_BEGIN and
// IC_API_PARSE_END)
#define IC_ARG_TEXT(ptr)                                                       \
    do {                                                                       \
        if (ic_args_parse_text(&_parser, ptr) != IC_OK) {                      \
            ic_api_trap("Failed to parse text argument");                      \
        }                                                                      \
    } while (0)

#define IC_ARG_NAT(ptr)                                                        \
    do {                                                                       \
        if (ic_args_parse_nat(&_parser, ptr) != IC_OK) {                       \
            ic_api_trap("Failed to parse nat argument");                       \
        }                                                                      \
    } while (0)

#define IC_ARG_INT(ptr)                                                        \
    do {                                                                       \
        if (ic_args_parse_int(&_parser, ptr) != IC_OK) {                       \
            ic_api_trap("Failed to parse int argument");                       \
        }                                                                      \
    } while (0)

#define IC_ARG_BOOL(ptr)                                                       \
    do {                                                                       \
        if (ic_args_parse_bool(&_parser, ptr) != IC_OK) {                      \
            ic_api_trap("Failed to parse bool argument");                      \
        }                                                                      \
    } while (0)

#define IC_ARG_BLOB(blob_ptr, len_ptr)                                         \
    do {                                                                       \
        uint8_t *_blob = NULL;                                                 \
        size_t   _len = 0;                                                     \
        if (ic_args_parse_blob(&_parser, &_blob, &_len) == IC_OK) {            \
            *(blob_ptr) = _blob;                                               \
            *(len_ptr) = _len;                                                 \
        } else {                                                               \
            ic_api_trap("Failed to parse blob argument");                      \
        }                                                                      \
    } while (0)

#define IC_ARG_PRINCIPAL(ptr)                                                  \
    do {                                                                       \
        ic_principal_t _principal;                                             \
        if (ic_args_parse_principal(&_parser, &_principal) == IC_OK) {         \
            *(ptr) = _principal;                                               \
        } else {                                                               \
            ic_api_trap("Failed to parse principal argument");                 \
        }                                                                      \
    } while (0)
