/*
 * Candid Header Parser and Writer
 *
 * Implements parsing and serialization of Candid message headers, which
 * contain:
 * - Magic bytes ("DIDL") for format identification
 * - Type table: Definitions of all composite types (records, variants, etc.)
 * - Argument types: Types of function arguments
 *
 * The header precedes the actual value data in Candid wire format. This module
 * handles the complex recursive parsing of type definitions including:
 * - Primitive types (nat, int, text, etc.)
 * - Composite types (opt, vec, record, variant, func, service)
 * - Type table references and forward declarations
 *
 * Used by both serialization (building headers) and deserialization (parsing
 * headers).
 */
#include "idl/header.h"

#include "idl/leb128.h"
#include "idl/type_table.h"
/* #include <stdio.h> */
#include <string.h>
#include <tinyprintf.h>

/* Helper to read ULEB128 from buffer */
static idl_status read_uleb128(const uint8_t *data,
                               size_t         data_len,
                               size_t        *pos,
                               uint64_t      *value) {
    size_t     consumed;
    idl_status st =
        idl_uleb128_decode(data + *pos, data_len - *pos, &consumed, value);
    if (st != IDL_STATUS_OK)
        return st;
    *pos += consumed;
    return IDL_STATUS_OK;
}

/* Helper to read SLEB128 from buffer */
static idl_status read_sleb128(const uint8_t *data,
                               size_t         data_len,
                               size_t        *pos,
                               int64_t       *value) {
    size_t     consumed;
    idl_status st =
        idl_sleb128_decode(data + *pos, data_len - *pos, &consumed, value);
    if (st != IDL_STATUS_OK)
        return st;
    *pos += consumed;
    return IDL_STATUS_OK;
}

/* Generate table variable name from index */
static const char *make_table_var(idl_arena *arena, int64_t index) {
    char buf[64];
    strcpy(buf, "table");
    tfp_snprintf(buf + 5, sizeof(buf) - 5, "%ld", (long)index);

    size_t len = strlen(buf) + 1;
    char  *name = idl_arena_alloc(arena, len);
    if (name) {
        memcpy(name, buf, len);
    }
    return name;
}

/* Convert wire index to type */
static idl_status index_to_type(idl_arena *arena,
                                int64_t    index,
                                uint64_t   table_len,
                                idl_type **out) {
    if (index >= 0) {
        /* Reference to type table entry */
        if ((uint64_t)index >= table_len) {
            return IDL_STATUS_ERR_INVALID_ARG;
        }
        *out = idl_type_var(arena, make_table_var(arena, index));
        return IDL_STATUS_OK;
    }

    /* Primitive type opcode */
    switch (index) {
    case IDL_TYPE_NULL:
        *out = idl_type_null(arena);
        break;
    case IDL_TYPE_BOOL:
        *out = idl_type_bool(arena);
        break;
    case IDL_TYPE_NAT:
        *out = idl_type_nat(arena);
        break;
    case IDL_TYPE_INT:
        *out = idl_type_int(arena);
        break;
    case IDL_TYPE_NAT8:
        *out = idl_type_nat8(arena);
        break;
    case IDL_TYPE_NAT16:
        *out = idl_type_nat16(arena);
        break;
    case IDL_TYPE_NAT32:
        *out = idl_type_nat32(arena);
        break;
    case IDL_TYPE_NAT64:
        *out = idl_type_nat64(arena);
        break;
    case IDL_TYPE_INT8:
        *out = idl_type_int8(arena);
        break;
    case IDL_TYPE_INT16:
        *out = idl_type_int16(arena);
        break;
    case IDL_TYPE_INT32:
        *out = idl_type_int32(arena);
        break;
    case IDL_TYPE_INT64:
        *out = idl_type_int64(arena);
        break;
    case IDL_TYPE_FLOAT32:
        *out = idl_type_float32(arena);
        break;
    case IDL_TYPE_FLOAT64:
        *out = idl_type_float64(arena);
        break;
    case IDL_TYPE_TEXT:
        *out = idl_type_text(arena);
        break;
    case IDL_TYPE_RESERVED:
        *out = idl_type_reserved(arena);
        break;
    case IDL_TYPE_EMPTY:
        *out = idl_type_empty(arena);
        break;
    case IDL_TYPE_PRINCIPAL:
        *out = idl_type_principal(arena);
        break;
    default:
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Parse OPT type */
static idl_status parse_opt_type(const uint8_t *data,
                                 size_t         data_len,
                                 size_t        *pos,
                                 idl_arena     *arena,
                                 uint64_t       table_len,
                                 idl_type     **out) {
    int64_t    inner_idx;
    idl_status st = read_sleb128(data, data_len, pos, &inner_idx);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_type *inner;
    st = index_to_type(arena, inner_idx, table_len, &inner);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    *out = idl_type_opt(arena, inner);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Parse VEC type */
static idl_status parse_vec_type(const uint8_t *data,
                                 size_t         data_len,
                                 size_t        *pos,
                                 idl_arena     *arena,
                                 uint64_t       table_len,
                                 idl_type     **out) {
    int64_t    inner_idx;
    idl_status st = read_sleb128(data, data_len, pos, &inner_idx);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_type *inner;
    st = index_to_type(arena, inner_idx, table_len, &inner);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    *out = idl_type_vec(arena, inner);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Parse a single record/variant field */
static idl_status parse_record_field(const uint8_t *data,
                                     size_t         data_len,
                                     size_t        *pos,
                                     idl_arena     *arena,
                                     uint64_t       table_len,
                                     uint32_t      *prev_id,
                                     idl_field     *field) {
    uint64_t   field_id;
    idl_status st = read_uleb128(data, data_len, pos, &field_id);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    if (field_id > UINT32_MAX) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    if (*prev_id > 0 && (uint32_t)field_id <= *prev_id) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    *prev_id = (uint32_t)field_id;

    int64_t type_idx;
    st = read_sleb128(data, data_len, pos, &type_idx);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_type *field_type;
    st = index_to_type(arena, type_idx, table_len, &field_type);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    field->label = idl_label_id((uint32_t)field_id);
    field->type = field_type;
    return IDL_STATUS_OK;
}

/* Parse RECORD or VARIANT type */
static idl_status parse_record_variant_type(const uint8_t *data,
                                            size_t         data_len,
                                            size_t        *pos,
                                            idl_arena     *arena,
                                            uint64_t       table_len,
                                            int64_t        opcode,
                                            idl_type     **out) {
    uint64_t   field_count;
    idl_status st = read_uleb128(data, data_len, pos, &field_count);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_field *fields = idl_arena_alloc(arena, sizeof(idl_field) * field_count);
    if (!fields && field_count > 0) {
        return IDL_STATUS_ERR_ALLOC;
    }

    uint32_t prev_id = 0;
    for (uint64_t i = 0; i < field_count; i++) {
        st = parse_record_field(data, data_len, pos, arena, table_len, &prev_id,
                                &fields[i]);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    if (opcode == IDL_TYPE_RECORD) {
        *out = idl_type_record(arena, fields, (size_t)field_count);
    } else {
        *out = idl_type_variant(arena, fields, (size_t)field_count);
    }
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Parse function arguments */
static idl_status parse_func_args(const uint8_t *data,
                                  size_t         data_len,
                                  size_t        *pos,
                                  idl_arena     *arena,
                                  uint64_t       table_len,
                                  idl_type    ***args,
                                  size_t        *args_len) {
    uint64_t   arg_count;
    idl_status st = read_uleb128(data, data_len, pos, &arg_count);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_type **arg_types =
        idl_arena_alloc(arena, sizeof(idl_type *) * arg_count);
    if (!arg_types && arg_count > 0) {
        return IDL_STATUS_ERR_ALLOC;
    }

    for (uint64_t i = 0; i < arg_count; i++) {
        int64_t type_idx;
        st = read_sleb128(data, data_len, pos, &type_idx);
        if (st != IDL_STATUS_OK) {
            return st;
        }

        st = index_to_type(arena, type_idx, table_len, &arg_types[i]);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    *args = arg_types;
    *args_len = (size_t)arg_count;
    return IDL_STATUS_OK;
}

/* Parse function returns */
static idl_status parse_func_rets(const uint8_t *data,
                                  size_t         data_len,
                                  size_t        *pos,
                                  idl_arena     *arena,
                                  uint64_t       table_len,
                                  idl_type    ***rets,
                                  size_t        *rets_len) {
    uint64_t   ret_count;
    idl_status st = read_uleb128(data, data_len, pos, &ret_count);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_type **ret_types =
        idl_arena_alloc(arena, sizeof(idl_type *) * ret_count);
    if (!ret_types && ret_count > 0) {
        return IDL_STATUS_ERR_ALLOC;
    }

    for (uint64_t i = 0; i < ret_count; i++) {
        int64_t type_idx;
        st = read_sleb128(data, data_len, pos, &type_idx);
        if (st != IDL_STATUS_OK) {
            return st;
        }

        st = index_to_type(arena, type_idx, table_len, &ret_types[i]);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    *rets = ret_types;
    *rets_len = (size_t)ret_count;
    return IDL_STATUS_OK;
}

/* Parse function modes */
static idl_status parse_func_modes(const uint8_t  *data,
                                   size_t          data_len,
                                   size_t         *pos,
                                   idl_arena      *arena,
                                   idl_func_mode **modes,
                                   size_t         *modes_len) {
    uint64_t   mode_count;
    idl_status st = read_uleb128(data, data_len, pos, &mode_count);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    if (mode_count > 1) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    idl_func_mode *mode_array = NULL;
    if (mode_count > 0) {
        mode_array = idl_arena_alloc(arena, sizeof(idl_func_mode) * mode_count);
        if (!mode_array) {
            return IDL_STATUS_ERR_ALLOC;
        }

        for (uint64_t i = 0; i < mode_count; i++) {
            uint64_t mode;
            st = read_uleb128(data, data_len, pos, &mode);
            if (st != IDL_STATUS_OK) {
                return st;
            }

            if (mode < 1 || mode > 3) {
                return IDL_STATUS_ERR_INVALID_ARG;
            }

            mode_array[i] = (idl_func_mode)mode;
        }
    }

    *modes = mode_array;
    *modes_len = (size_t)mode_count;
    return IDL_STATUS_OK;
}

/* Parse FUNC type */
static idl_status parse_func_type(const uint8_t *data,
                                  size_t         data_len,
                                  size_t        *pos,
                                  idl_arena     *arena,
                                  uint64_t       table_len,
                                  idl_type     **out) {
    idl_type **args;
    size_t     args_len;
    idl_status st = parse_func_args(data, data_len, pos, arena, table_len,
                                    &args, &args_len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_type **rets;
    size_t     rets_len;
    st = parse_func_rets(data, data_len, pos, arena, table_len, &rets,
                         &rets_len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_func_mode *modes;
    size_t         modes_len;
    st = parse_func_modes(data, data_len, pos, arena, &modes, &modes_len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_func func = {
        .args = args,
        .args_len = args_len,
        .rets = rets,
        .rets_len = rets_len,
        .modes = modes,
        .modes_len = modes_len,
    };

    *out = idl_type_func(arena, &func);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Parse a single service method */
static idl_status parse_service_method(const uint8_t *data,
                                       size_t         data_len,
                                       size_t        *pos,
                                       idl_arena     *arena,
                                       uint64_t       table_len,
                                       const char   **prev_name,
                                       idl_method    *method) {
    uint64_t   name_len;
    idl_status st = read_uleb128(data, data_len, pos, &name_len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    if (*pos + name_len > data_len) {
        return IDL_STATUS_ERR_TRUNCATED;
    }

    char *name = idl_arena_alloc(arena, name_len + 1);
    if (!name) {
        return IDL_STATUS_ERR_ALLOC;
    }
    memcpy(name, data + *pos, name_len);
    name[name_len] = '\0';
    *pos += name_len;

    if (*prev_name && strcmp(*prev_name, name) >= 0) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    *prev_name = name;

    int64_t type_idx;
    st = read_sleb128(data, data_len, pos, &type_idx);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_type *method_type;
    st = index_to_type(arena, type_idx, table_len, &method_type);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    method->name = name;
    method->type = method_type;
    return IDL_STATUS_OK;
}

/* Parse SERVICE type */
static idl_status parse_service_type(const uint8_t *data,
                                     size_t         data_len,
                                     size_t        *pos,
                                     idl_arena     *arena,
                                     uint64_t       table_len,
                                     idl_type     **out) {
    uint64_t   method_count;
    idl_status st = read_uleb128(data, data_len, pos, &method_count);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    idl_method *methods =
        idl_arena_alloc(arena, sizeof(idl_method) * method_count);
    if (!methods && method_count > 0) {
        return IDL_STATUS_ERR_ALLOC;
    }

    const char *prev_name = NULL;
    for (uint64_t i = 0; i < method_count; i++) {
        st = parse_service_method(data, data_len, pos, arena, table_len,
                                  &prev_name, &methods[i]);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    idl_service service = {
        .methods = methods,
        .methods_len = (size_t)method_count,
    };

    *out = idl_type_service(arena, &service);
    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

/* Parse unknown/future type */
static idl_status parse_unknown_type(const uint8_t *data,
                                     size_t         data_len,
                                     size_t        *pos,
                                     idl_arena     *arena,
                                     int64_t        opcode,
                                     idl_type     **out) {
    if (opcode < -24) {
        uint64_t   blob_len;
        idl_status st = read_uleb128(data, data_len, pos, &blob_len);
        if (st != IDL_STATUS_OK) {
            return st;
        }

        if (*pos + blob_len > data_len) {
            return IDL_STATUS_ERR_TRUNCATED;
        }

        *pos += blob_len;
        *out = idl_type_reserved(arena);
        return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
    }

    return IDL_STATUS_ERR_INVALID_ARG;
}

/* Parse a composite type from the type table */
static idl_status parse_cons_type(const uint8_t *data,
                                  size_t         data_len,
                                  size_t        *pos,
                                  idl_arena     *arena,
                                  uint64_t       table_len,
                                  idl_type     **out) {
    int64_t    opcode;
    idl_status st = read_sleb128(data, data_len, pos, &opcode);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    switch (opcode) {
    case IDL_TYPE_OPT:
        return parse_opt_type(data, data_len, pos, arena, table_len, out);

    case IDL_TYPE_VEC:
        return parse_vec_type(data, data_len, pos, arena, table_len, out);

    case IDL_TYPE_RECORD:
    case IDL_TYPE_VARIANT:
        return parse_record_variant_type(data, data_len, pos, arena, table_len,
                                         opcode, out);

    case IDL_TYPE_FUNC:
        return parse_func_type(data, data_len, pos, arena, table_len, out);

    case IDL_TYPE_SERVICE:
        return parse_service_type(data, data_len, pos, arena, table_len, out);

    default:
        return parse_unknown_type(data, data_len, pos, arena, opcode, out);
    }
}

idl_status idl_header_parse(const uint8_t *data,
                            size_t         data_len,
                            idl_arena     *arena,
                            idl_header    *header,
                            size_t        *consumed) {
    if (!data || !arena || !header) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    size_t pos = 0;

    /* Check magic bytes "DIDL" */
    if (data_len < 4) {
        return IDL_STATUS_ERR_TRUNCATED;
    }
    if (data[0] != IDL_MAGIC_0 || data[1] != IDL_MAGIC_1 ||
        data[2] != IDL_MAGIC_2 || data[3] != IDL_MAGIC_3) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    pos = 4;

    /* Initialize type environment */
    idl_status st = idl_type_env_init(&header->env, arena);
    if (st != IDL_STATUS_OK)
        return st;

    /* Read type table length */
    uint64_t table_len;
    st = read_uleb128(data, data_len, &pos, &table_len);
    if (st != IDL_STATUS_OK)
        return st;

    /* Parse type table entries */
    for (uint64_t i = 0; i < table_len; i++) {
        idl_type *type;
        st = parse_cons_type(data, data_len, &pos, arena, table_len, &type);
        if (st != IDL_STATUS_OK)
            return st;

        const char *name = make_table_var(arena, (int64_t)i);
        if (!name)
            return IDL_STATUS_ERR_ALLOC;

        st = idl_type_env_insert(&header->env, name, type);
        if (st != IDL_STATUS_OK)
            return st;
    }

    /* Read argument count */
    uint64_t arg_count;
    st = read_uleb128(data, data_len, &pos, &arg_count);
    if (st != IDL_STATUS_OK)
        return st;

    /* Allocate argument types */
    header->arg_types = idl_arena_alloc(arena, sizeof(idl_type *) * arg_count);
    if (!header->arg_types && arg_count > 0)
        return IDL_STATUS_ERR_ALLOC;
    header->arg_count = (size_t)arg_count;

    /* Parse argument types */
    for (uint64_t i = 0; i < arg_count; i++) {
        int64_t type_idx;
        st = read_sleb128(data, data_len, &pos, &type_idx);
        if (st != IDL_STATUS_OK)
            return st;

        st = index_to_type(arena, type_idx, table_len, &header->arg_types[i]);
        if (st != IDL_STATUS_OK)
            return st;
    }

    if (consumed) {
        *consumed = pos;
    }

    return IDL_STATUS_OK;
}

idl_status idl_header_write(idl_type_table_builder *builder,
                            idl_arena              *arena,
                            uint8_t               **out,
                            size_t                 *out_len) {
    if (!builder || !arena || !out || !out_len) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    /* Serialize type table and args */
    uint8_t   *type_data;
    size_t     type_len;
    idl_status st =
        idl_type_table_builder_serialize(builder, &type_data, &type_len);
    if (st != IDL_STATUS_OK)
        return st;

    /* Allocate output: "DIDL" + type_data */
    size_t   total_len = 4 + type_len;
    uint8_t *buf = idl_arena_alloc(arena, total_len);
    if (!buf)
        return IDL_STATUS_ERR_ALLOC;

    /* Write magic */
    buf[0] = IDL_MAGIC_0;
    buf[1] = IDL_MAGIC_1;
    buf[2] = IDL_MAGIC_2;
    buf[3] = IDL_MAGIC_3;

    /* Copy type data */
    memcpy(buf + 4, type_data, type_len);

    *out = buf;
    *out_len = total_len;

    return IDL_STATUS_OK;
}
