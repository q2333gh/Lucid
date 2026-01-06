// Deserializer Configuration and Initialization
#include "idl/deserializer.h"
#include "idl/header.h"

void idl_decoder_config_init(idl_decoder_config *config) {
    if (!config) {
        return;
    }
    config->decoding_quota = 0;
    config->skipping_quota = 0;
    config->full_error_message = 1;
}

idl_decoder_config *
idl_decoder_config_set_decoding_quota(idl_decoder_config *config,
                                      size_t              quota) {
    if (config) {
        config->decoding_quota = quota;
    }
    return config;
}

idl_decoder_config *
idl_decoder_config_set_skipping_quota(idl_decoder_config *config,
                                      size_t              quota) {
    if (config) {
        config->skipping_quota = quota;
    }
    return config;
}

idl_status idl_deserializer_new_with_config(const uint8_t            *data,
                                            size_t                    len,
                                            idl_arena                *arena,
                                            const idl_decoder_config *config,
                                            idl_deserializer        **out) {
    if (!data || !arena || !out) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    idl_deserializer *deserializer =
        idl_arena_alloc_zeroed(arena, sizeof(idl_deserializer));
    if (!deserializer) {
        return IDL_STATUS_ERR_ALLOC;
    }

    deserializer->arena = arena;
    deserializer->input = data;
    deserializer->input_len = len;
    deserializer->pos = 0;
    deserializer->arg_index = 0;

    if (config) {
        deserializer->config = *config;
    } else {
        idl_decoder_config_init(&deserializer->config);
    }

    size_t     consumed;
    idl_status st =
        idl_header_parse(data, len, arena, &deserializer->header, &consumed);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    deserializer->pos = consumed;
    deserializer->cost_accumulated = 0;

    idl_status cost_st = idl_deserializer_add_cost(deserializer, consumed * 4);
    if (cost_st != IDL_STATUS_OK) {
        return cost_st;
    }

    *out = deserializer;
    return IDL_STATUS_OK;
}

idl_status idl_deserializer_new(const uint8_t     *data,
                                size_t             len,
                                idl_arena         *arena,
                                idl_deserializer **out) {
    return idl_deserializer_new_with_config(data, len, arena, NULL, out);
}

int idl_deserializer_is_done(const idl_deserializer *de) {
    return de ? (de->arg_index >= de->header.arg_count) : 1;
}

idl_status idl_deserializer_done(idl_deserializer *de) {
    if (!de) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    while (!idl_deserializer_is_done(de)) {
        idl_type  *type;
        idl_value *value;
        idl_status st = idl_deserializer_get_value(de, &type, &value);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    if (de->pos < de->input_len) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    return IDL_STATUS_OK;
}

size_t idl_deserializer_remaining(const idl_deserializer *de) {
    return de ? (de->input_len - de->pos) : 0;
}
