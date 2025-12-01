#include "idl/header.h"

#include "idl/leb128.h"
#include "idl/type_table.h"
/* #include <stdio.h> */
#include <string.h>

/* Helper to read ULEB128 from buffer */
static idl_status read_uleb128(const uint8_t *data, size_t data_len, size_t *pos, uint64_t *value) {
    size_t     consumed;
    idl_status st = idl_uleb128_decode(data + *pos, data_len - *pos, &consumed, value);
    if (st != IDL_STATUS_OK)
        return st;
    *pos += consumed;
    return IDL_STATUS_OK;
}

/* Helper to read SLEB128 from buffer */
static idl_status read_sleb128(const uint8_t *data, size_t data_len, size_t *pos, int64_t *value) {
    size_t     consumed;
    idl_status st = idl_sleb128_decode(data + *pos, data_len - *pos, &consumed, value);
    if (st != IDL_STATUS_OK)
        return st;
    *pos += consumed;
    return IDL_STATUS_OK;
}

/* Helper to stringify int */
static void simple_int_to_str(long val, char *buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char temp[24];
    int  i = 0;
    int  neg = 0;
    if (val < 0) {
        neg = 1;
        val = -val;
    }
    while (val > 0) {
        temp[i++] = '0' + (val % 10);
        val /= 10;
    }
    if (neg)
        *buf++ = '-';
    while (i > 0)
        *buf++ = temp[--i];
    *buf = '\0';
}

/* Generate table variable name from index */
static const char *make_table_var(idl_arena *arena, int64_t index) {
    char buf[64];
    strcpy(buf, "table");
    simple_int_to_str((long)index, buf + 5);

    size_t len = strlen(buf) + 1;
    char  *name = idl_arena_alloc(arena, len);
    if (name) {
        memcpy(name, buf, len);
    }
    return name;
}

/* Convert wire index to type */
static idl_status
index_to_type(idl_arena *arena, int64_t index, uint64_t table_len, idl_type **out) {
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

/* Parse a composite type from the type table */
static idl_status parse_cons_type(const uint8_t *data,
                                  size_t         data_len,
                                  size_t        *pos,
                                  idl_arena     *arena,
                                  uint64_t       table_len,
                                  idl_type     **out) {
    int64_t    opcode;
    idl_status st = read_sleb128(data, data_len, pos, &opcode);
    if (st != IDL_STATUS_OK)
        return st;

    switch (opcode) {
    case IDL_TYPE_OPT: {
        int64_t inner_idx;
        st = read_sleb128(data, data_len, pos, &inner_idx);
        if (st != IDL_STATUS_OK)
            return st;

        idl_type *inner;
        st = index_to_type(arena, inner_idx, table_len, &inner);
        if (st != IDL_STATUS_OK)
            return st;

        *out = idl_type_opt(arena, inner);
        break;
    }

    case IDL_TYPE_VEC: {
        int64_t inner_idx;
        st = read_sleb128(data, data_len, pos, &inner_idx);
        if (st != IDL_STATUS_OK)
            return st;

        idl_type *inner;
        st = index_to_type(arena, inner_idx, table_len, &inner);
        if (st != IDL_STATUS_OK)
            return st;

        *out = idl_type_vec(arena, inner);
        break;
    }

    case IDL_TYPE_RECORD:
    case IDL_TYPE_VARIANT: {
        uint64_t field_count;
        st = read_uleb128(data, data_len, pos, &field_count);
        if (st != IDL_STATUS_OK)
            return st;

        idl_field *fields = idl_arena_alloc(arena, sizeof(idl_field) * field_count);
        if (!fields && field_count > 0)
            return IDL_STATUS_ERR_ALLOC;

        uint32_t prev_id = 0;
        for (uint64_t i = 0; i < field_count; i++) {
            uint64_t field_id;
            st = read_uleb128(data, data_len, pos, &field_id);
            if (st != IDL_STATUS_OK)
                return st;

            if (field_id > UINT32_MAX)
                return IDL_STATUS_ERR_INVALID_ARG;

            /* Check sorting and uniqueness */
            if (i > 0 && (uint32_t)field_id <= prev_id) {
                return IDL_STATUS_ERR_INVALID_ARG;
            }
            prev_id = (uint32_t)field_id;

            int64_t type_idx;
            st = read_sleb128(data, data_len, pos, &type_idx);
            if (st != IDL_STATUS_OK)
                return st;

            idl_type *field_type;
            st = index_to_type(arena, type_idx, table_len, &field_type);
            if (st != IDL_STATUS_OK)
                return st;

            fields[i].label = idl_label_id((uint32_t)field_id);
            fields[i].type = field_type;
        }

        if (opcode == IDL_TYPE_RECORD) {
            *out = idl_type_record(arena, fields, (size_t)field_count);
        } else {
            *out = idl_type_variant(arena, fields, (size_t)field_count);
        }
        break;
    }

    case IDL_TYPE_FUNC: {
        uint64_t arg_count;
        st = read_uleb128(data, data_len, pos, &arg_count);
        if (st != IDL_STATUS_OK)
            return st;

        idl_type **args = idl_arena_alloc(arena, sizeof(idl_type *) * arg_count);
        if (!args && arg_count > 0)
            return IDL_STATUS_ERR_ALLOC;

        for (uint64_t i = 0; i < arg_count; i++) {
            int64_t type_idx;
            st = read_sleb128(data, data_len, pos, &type_idx);
            if (st != IDL_STATUS_OK)
                return st;

            st = index_to_type(arena, type_idx, table_len, &args[i]);
            if (st != IDL_STATUS_OK)
                return st;
        }

        uint64_t ret_count;
        st = read_uleb128(data, data_len, pos, &ret_count);
        if (st != IDL_STATUS_OK)
            return st;

        idl_type **rets = idl_arena_alloc(arena, sizeof(idl_type *) * ret_count);
        if (!rets && ret_count > 0)
            return IDL_STATUS_ERR_ALLOC;

        for (uint64_t i = 0; i < ret_count; i++) {
            int64_t type_idx;
            st = read_sleb128(data, data_len, pos, &type_idx);
            if (st != IDL_STATUS_OK)
                return st;

            st = index_to_type(arena, type_idx, table_len, &rets[i]);
            if (st != IDL_STATUS_OK)
                return st;
        }

        uint64_t mode_count;
        st = read_uleb128(data, data_len, pos, &mode_count);
        if (st != IDL_STATUS_OK)
            return st;

        if (mode_count > 1)
            return IDL_STATUS_ERR_INVALID_ARG;

        idl_func_mode *modes = NULL;
        if (mode_count > 0) {
            modes = idl_arena_alloc(arena, sizeof(idl_func_mode) * mode_count);
            if (!modes)
                return IDL_STATUS_ERR_ALLOC;

            for (uint64_t i = 0; i < mode_count; i++) {
                uint64_t mode;
                st = read_uleb128(data, data_len, pos, &mode);
                if (st != IDL_STATUS_OK)
                    return st;

                if (mode < 1 || mode > 3)
                    return IDL_STATUS_ERR_INVALID_ARG;

                modes[i] = (idl_func_mode)mode;
            }
        }

        idl_func func = {
            .args = args,
            .args_len = (size_t)arg_count,
            .rets = rets,
            .rets_len = (size_t)ret_count,
            .modes = modes,
            .modes_len = (size_t)mode_count,
        };

        *out = idl_type_func(arena, &func);
        break;
    }

    case IDL_TYPE_SERVICE: {
        uint64_t method_count;
        st = read_uleb128(data, data_len, pos, &method_count);
        if (st != IDL_STATUS_OK)
            return st;

        idl_method *methods = idl_arena_alloc(arena, sizeof(idl_method) * method_count);
        if (!methods && method_count > 0)
            return IDL_STATUS_ERR_ALLOC;

        const char *prev_name = NULL;
        for (uint64_t i = 0; i < method_count; i++) {
            uint64_t name_len;
            st = read_uleb128(data, data_len, pos, &name_len);
            if (st != IDL_STATUS_OK)
                return st;

            if (*pos + name_len > data_len)
                return IDL_STATUS_ERR_TRUNCATED;

            char *name = idl_arena_alloc(arena, name_len + 1);
            if (!name)
                return IDL_STATUS_ERR_ALLOC;
            memcpy(name, data + *pos, name_len);
            name[name_len] = '\0';
            *pos += name_len;

            /* Check sorting and uniqueness */
            if (prev_name && strcmp(prev_name, name) >= 0) {
                return IDL_STATUS_ERR_INVALID_ARG;
            }
            prev_name = name;

            int64_t type_idx;
            st = read_sleb128(data, data_len, pos, &type_idx);
            if (st != IDL_STATUS_OK)
                return st;

            idl_type *method_type;
            st = index_to_type(arena, type_idx, table_len, &method_type);
            if (st != IDL_STATUS_OK)
                return st;

            methods[i].name = name;
            methods[i].type = method_type;
        }

        idl_service service = {
            .methods = methods,
            .methods_len = (size_t)method_count,
        };

        *out = idl_type_service(arena, &service);
        break;
    }

    default:
        /* Unknown or future type - skip blob */
        if (opcode < -24) {
            uint64_t blob_len;
            st = read_uleb128(data, data_len, pos, &blob_len);
            if (st != IDL_STATUS_OK)
                return st;

            if (*pos + blob_len > data_len)
                return IDL_STATUS_ERR_TRUNCATED;

            *pos += blob_len;
            /* Create a reserved type as placeholder */
            *out = idl_type_reserved(arena);
        } else {
            return IDL_STATUS_ERR_INVALID_ARG;
        }
        break;
    }

    return *out ? IDL_STATUS_OK : IDL_STATUS_ERR_ALLOC;
}

idl_status idl_header_parse(
    const uint8_t *data, size_t data_len, idl_arena *arena, idl_header *header, size_t *consumed) {
    if (!data || !arena || !header) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    size_t pos = 0;

    /* Check magic bytes "DIDL" */
    if (data_len < 4) {
        return IDL_STATUS_ERR_TRUNCATED;
    }
    if (data[0] != IDL_MAGIC_0 || data[1] != IDL_MAGIC_1 || data[2] != IDL_MAGIC_2 ||
        data[3] != IDL_MAGIC_3) {
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
    idl_status st = idl_type_table_builder_serialize(builder, &type_data, &type_len);
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
