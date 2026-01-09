/*
 * Simplified Argument Parser
 *
 * Provides a simplified API for parsing Candid arguments from incoming
 * messages. Features:
 * - Single initialization with automatic deserializer setup
 * - Type-safe parsing functions for common types (text, nat, int, bool, blob,
 * principal)
 * - Automatic memory allocation for parsed values (managed by ic_api memory
 * pool)
 * - Reuses deserializer from ic_api context for efficiency
 * - Prevents double-parsing with initialization guards
 *
 * This is a convenience layer over the lower-level ic_api_from_wire functions,
 * providing a more ergonomic interface for argument extraction.
 */
#include "ic_args.h"

#include <stdlib.h>
#include <string.h>

#include "ic_buffer.h"
#include "ic_entry_points.h"
#include "ic_principal.h"
#include "ic_types.h"
#include "idl/candid.h"

// Forward declaration removed - function is declared in ic_api.h

// Local structure definition matching ic_api_t for direct field access
// This matches the structure definition in ic_api.c
// We need this to access internal fields without exporting functions to WASM
struct ic_api_local {
    ic_entry_type_t   entry_type;
    const char       *calling_function;
    ic_buffer_t       input_buffer;
    ic_buffer_t       output_buffer;
    ic_principal_t    caller;
    ic_principal_t    canister_self;
    bool              debug_print;
    bool              called_from_wire;
    bool              called_to_wire;
    idl_arena         arena;
    void             *mem_pool; // ic_mem_node_t*
    idl_deserializer *deserializer;
};

// Internal accessor functions (static, not exported to WASM)
static inline idl_deserializer *ic_args_get_deserializer(ic_api_t *api) {
    return ((struct ic_api_local *)api)->deserializer;
}

static inline void ic_args_set_deserializer(ic_api_t         *api,
                                            idl_deserializer *de) {
    ((struct ic_api_local *)api)->deserializer = de;
}

static inline bool ic_args_get_called_from_wire(ic_api_t *api) {
    return ((struct ic_api_local *)api)->called_from_wire;
}

static inline void ic_args_set_called_from_wire(ic_api_t *api, bool value) {
    ((struct ic_api_local *)api)->called_from_wire = value;
}

// Initialize argument parser
ic_result_t ic_args_parser_init(ic_args_parser_t *parser, ic_api_t *api) {
    if (parser == NULL || api == NULL) {
        return IC_ERR_INVALID_ARG;
    }

    // Check if already initialized
    idl_deserializer *existing_de = ic_args_get_deserializer(api);
    if (existing_de != NULL) {
        parser->api = api;
        parser->deserializer = existing_de;
        parser->initialized = true;
        return IC_OK;
    }

    // Check if already called from_wire
    if (ic_args_get_called_from_wire(api)) {
        return IC_ERR_INVALID_ARG; // Already parsed
    }

    // Initialize deserializer
    const ic_buffer_t *input = ic_api_get_input_buffer(api);
    const uint8_t     *data = ic_buffer_data(input);
    size_t             len = ic_buffer_size(input);

    idl_arena        *arena = ic_api_get_arena_internal(api);
    idl_deserializer *de = NULL;
    if (idl_deserializer_new(data, len, arena, &de) != IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    ic_args_set_deserializer(api, de);
    ic_args_set_called_from_wire(api, true);

    parser->api = api;
    parser->deserializer = de;
    parser->initialized = true;

    return IC_OK;
}

// Parse text argument (with automatic memory allocation)
ic_result_t ic_args_parse_text(ic_args_parser_t *parser, char **text) {
    if (parser == NULL || text == NULL || !parser->initialized) {
        return IC_ERR_INVALID_ARG;
    }

    idl_type  *type = NULL;
    idl_value *val = NULL;
    if (idl_deserializer_get_value(parser->deserializer, &type, &val) !=
        IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    if (val->kind != IDL_VALUE_TEXT) {
        return IC_ERR_INVALID_ARG;
    }

    // Allocate memory and copy string (with null terminator)
    size_t len = val->data.text.len;
    char  *copy = (char *)ic_api_malloc(parser->api, len + 1);
    if (copy == NULL) {
        return IC_ERR_OUT_OF_MEMORY;
    }

    memcpy(copy, val->data.text.data, len);
    copy[len] = '\0';

    *text = copy;
    return IC_OK;
}

// Parse nat argument
ic_result_t ic_args_parse_nat(ic_args_parser_t *parser, uint64_t *value) {
    if (parser == NULL || value == NULL || !parser->initialized) {
        return IC_ERR_INVALID_ARG;
    }

    idl_type  *type = NULL;
    idl_value *val = NULL;
    if (idl_deserializer_get_value(parser->deserializer, &type, &val) !=
        IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    if (val->kind == IDL_VALUE_NAT64) {
        *value = val->data.nat64_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_NAT32) {
        *value = val->data.nat32_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_NAT16) {
        *value = val->data.nat16_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_NAT8) {
        *value = val->data.nat8_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_NAT) {
        // Try to decode LEB128
        size_t consumed;
        if (idl_uleb128_decode(val->data.bignum.data, val->data.bignum.len,
                               &consumed, value) != IDL_STATUS_OK) {
            return IC_ERR_INVALID_ARG;
        }
        return IC_OK;
    }

    return IC_ERR_INVALID_ARG;
}

// Parse int argument
ic_result_t ic_args_parse_int(ic_args_parser_t *parser, int64_t *value) {
    if (parser == NULL || value == NULL || !parser->initialized) {
        return IC_ERR_INVALID_ARG;
    }

    idl_type  *type = NULL;
    idl_value *val = NULL;
    if (idl_deserializer_get_value(parser->deserializer, &type, &val) !=
        IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    if (val->kind == IDL_VALUE_INT64) {
        *value = val->data.int64_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_INT32) {
        *value = val->data.int32_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_INT16) {
        *value = val->data.int16_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_INT8) {
        *value = val->data.int8_val;
        return IC_OK;
    }
    if (val->kind == IDL_VALUE_INT) {
        size_t consumed;
        if (idl_sleb128_decode(val->data.bignum.data, val->data.bignum.len,
                               &consumed, value) != IDL_STATUS_OK) {
            return IC_ERR_INVALID_ARG;
        }
        return IC_OK;
    }

    return IC_ERR_INVALID_ARG;
}

// Parse bool argument
ic_result_t ic_args_parse_bool(ic_args_parser_t *parser, bool *value) {
    if (parser == NULL || value == NULL || !parser->initialized) {
        return IC_ERR_INVALID_ARG;
    }

    idl_type  *type = NULL;
    idl_value *val = NULL;
    if (idl_deserializer_get_value(parser->deserializer, &type, &val) !=
        IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    if (val->kind != IDL_VALUE_BOOL) {
        return IC_ERR_INVALID_ARG;
    }

    *value = (val->data.bool_val != 0);
    return IC_OK;
}

// Parse blob argument (with automatic memory allocation)
ic_result_t
ic_args_parse_blob(ic_args_parser_t *parser, uint8_t **blob, size_t *blob_len) {
    if (parser == NULL || blob == NULL || blob_len == NULL ||
        !parser->initialized) {
        return IC_ERR_INVALID_ARG;
    }

    idl_type  *type = NULL;
    idl_value *val = NULL;
    if (idl_deserializer_get_value(parser->deserializer, &type, &val) !=
        IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    if (val->kind != IDL_VALUE_BLOB) {
        return IC_ERR_INVALID_ARG;
    }

    // Allocate memory and copy blob
    size_t   len = val->data.blob.len;
    uint8_t *copy = (uint8_t *)ic_api_malloc(parser->api, len);
    if (copy == NULL) {
        return IC_ERR_OUT_OF_MEMORY;
    }

    memcpy(copy, val->data.blob.data, len);

    *blob = copy;
    *blob_len = len;
    return IC_OK;
}

// Parse principal argument
ic_result_t ic_args_parse_principal(ic_args_parser_t *parser,
                                    ic_principal_t   *principal) {
    if (parser == NULL || principal == NULL || !parser->initialized) {
        return IC_ERR_INVALID_ARG;
    }

    idl_type  *type = NULL;
    idl_value *val = NULL;
    if (idl_deserializer_get_value(parser->deserializer, &type, &val) !=
        IDL_STATUS_OK) {
        return IC_ERR_INVALID_ARG;
    }

    if (val->kind != IDL_VALUE_PRINCIPAL) {
        return IC_ERR_INVALID_ARG;
    }

    return ic_principal_from_bytes(principal, val->data.principal.data,
                                   val->data.principal.len);
}
