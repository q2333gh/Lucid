#include "idl/type_table.h"

#include <string.h>

#include "idl/leb128.h"

#define INITIAL_CAPACITY 16
#define MAX_LEB128_SIZE 10

/* Grow a dynamic array */
static idl_status
grow_array(idl_arena *arena, void **arr, size_t *cap, size_t elem_size) {
    size_t new_cap = (*cap == 0) ? INITIAL_CAPACITY : (*cap * 2);
    void  *new_arr = idl_arena_alloc(arena, new_cap * elem_size);
    if (!new_arr) {
        return IDL_STATUS_ERR_ALLOC;
    }
    if (*arr && *cap > 0) {
        memcpy(new_arr, *arr, *cap * elem_size);
    }
    *arr = new_arr;
    *cap = new_cap;
    return IDL_STATUS_OK;
}

idl_status idl_type_table_builder_init(idl_type_table_builder *builder,
                                       idl_arena              *arena,
                                       idl_type_env           *env) {
    if (!builder || !arena) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    memset(builder, 0, sizeof(*builder));
    builder->arena = arena;
    builder->env = env;

    return IDL_STATUS_OK;
}

/* Find type in the type map */
static int find_type_index(const idl_type_table_builder *builder,
                           const idl_type               *type,
                           int32_t                      *out) {
    for (size_t i = 0; i < builder->type_map_count; i++) {
        if (builder->type_keys[i] == type) {
            *out = builder->type_indices[i];
            return 1;
        }
    }
    return 0;
}

/* Add type to the type map */
static idl_status add_type_mapping(idl_type_table_builder *builder,
                                   idl_type               *type,
                                   int32_t                 index) {
    if (builder->type_map_count >= builder->type_map_capacity) {
        idl_status st;
        st = grow_array(builder->arena, (void **)&builder->type_keys,
                        &builder->type_map_capacity, sizeof(idl_type *));
        if (st != IDL_STATUS_OK)
            return st;

        /* Also grow indices array to match */
        size_t   old_cap = builder->type_map_capacity / 2;
        int32_t *new_indices = idl_arena_alloc(
            builder->arena, builder->type_map_capacity * sizeof(int32_t));
        if (!new_indices)
            return IDL_STATUS_ERR_ALLOC;
        if (builder->type_indices && old_cap > 0) {
            memcpy(new_indices, builder->type_indices,
                   old_cap * sizeof(int32_t));
        }
        builder->type_indices = new_indices;
    }

    builder->type_keys[builder->type_map_count] = type;
    builder->type_indices[builder->type_map_count] = index;
    builder->type_map_count++;

    return IDL_STATUS_OK;
}

/* Write SLEB128 to a dynamic buffer */
static idl_status write_sleb128(
    idl_arena *arena, uint8_t **buf, size_t *len, size_t *cap, int64_t value) {
    uint8_t    tmp[MAX_LEB128_SIZE];
    size_t     written;
    idl_status st = idl_sleb128_encode(value, tmp, sizeof(tmp), &written);
    if (st != IDL_STATUS_OK)
        return st;

    while (*len + written > *cap) {
        size_t   new_cap = (*cap == 0) ? 64 : (*cap * 2);
        uint8_t *new_buf = idl_arena_alloc(arena, new_cap);
        if (!new_buf)
            return IDL_STATUS_ERR_ALLOC;
        if (*buf && *cap > 0) {
            memcpy(new_buf, *buf, *len);
        }
        *buf = new_buf;
        *cap = new_cap;
    }

    memcpy(*buf + *len, tmp, written);
    *len += written;
    return IDL_STATUS_OK;
}

/* Write ULEB128 to a dynamic buffer */
static idl_status write_uleb128(
    idl_arena *arena, uint8_t **buf, size_t *len, size_t *cap, uint64_t value) {
    uint8_t    tmp[MAX_LEB128_SIZE];
    size_t     written;
    idl_status st = idl_uleb128_encode(value, tmp, sizeof(tmp), &written);
    if (st != IDL_STATUS_OK)
        return st;

    while (*len + written > *cap) {
        size_t   new_cap = (*cap == 0) ? 64 : (*cap * 2);
        uint8_t *new_buf = idl_arena_alloc(arena, new_cap);
        if (!new_buf)
            return IDL_STATUS_ERR_ALLOC;
        if (*buf && *cap > 0) {
            memcpy(new_buf, *buf, *len);
        }
        *buf = new_buf;
        *cap = new_cap;
    }

    memcpy(*buf + *len, tmp, written);
    *len += written;
    return IDL_STATUS_OK;
}

/* Encode a type reference (opcode or index) */
static idl_status encode_type_ref(const idl_type_table_builder *builder,
                                  idl_arena                    *arena,
                                  uint8_t                     **buf,
                                  size_t                       *len,
                                  size_t                       *cap,
                                  const idl_type               *type) {
    int32_t index;

    if (idl_type_is_primitive(type)) {
        /* Primitive types use their opcode directly */
        return write_sleb128(arena, buf, len, cap, idl_type_opcode(type->kind));
    }

    if (type->kind == IDL_KIND_VAR && builder->env) {
        /* Look up the VAR in the environment and get its index */
        idl_type *resolved =
            idl_type_env_find(builder->env, type->data.var_name);
        if (resolved && find_type_index(builder, resolved, &index)) {
            return write_sleb128(arena, buf, len, cap, index);
        }
        /* If not found, try the VAR type itself */
        if (find_type_index(builder, type, &index)) {
            return write_sleb128(arena, buf, len, cap, index);
        }
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    if (find_type_index(builder, type, &index)) {
        return write_sleb128(arena, buf, len, cap, index);
    }

    return IDL_STATUS_ERR_INVALID_ARG;
}

/* Build OPT type entry */
static idl_status build_opt_type_entry(idl_type_table_builder *builder,
                                       idl_type               *type,
                                       uint8_t               **buf,
                                       size_t                 *len,
                                       size_t                 *cap) {
    idl_status st =
        idl_type_table_builder_build_type(builder, type->data.inner, NULL);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    st = write_sleb128(builder->arena, buf, len, cap, IDL_TYPE_OPT);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    return encode_type_ref(builder, builder->arena, buf, len, cap,
                           type->data.inner);
}

/* Build VEC type entry */
static idl_status build_vec_type_entry(idl_type_table_builder *builder,
                                       idl_type               *type,
                                       uint8_t               **buf,
                                       size_t                 *len,
                                       size_t                 *cap) {
    idl_status st =
        idl_type_table_builder_build_type(builder, type->data.inner, NULL);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    st = write_sleb128(builder->arena, buf, len, cap, IDL_TYPE_VEC);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    return encode_type_ref(builder, builder->arena, buf, len, cap,
                           type->data.inner);
}

/* Build RECORD or VARIANT type entry */
static idl_status build_record_variant_entry(idl_type_table_builder *builder,
                                             idl_type               *type,
                                             uint8_t               **buf,
                                             size_t                 *len,
                                             size_t                 *cap) {
    for (size_t i = 0; i < type->data.record.fields_len; i++) {
        idl_status st = idl_type_table_builder_build_type(
            builder, type->data.record.fields[i].type, NULL);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    int64_t opcode =
        (type->kind == IDL_KIND_RECORD) ? IDL_TYPE_RECORD : IDL_TYPE_VARIANT;
    idl_status st = write_sleb128(builder->arena, buf, len, cap, opcode);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    st = write_uleb128(builder->arena, buf, len, cap,
                       type->data.record.fields_len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    for (size_t i = 0; i < type->data.record.fields_len; i++) {
        idl_field *f = &type->data.record.fields[i];
        st = write_uleb128(builder->arena, buf, len, cap, f->label.id);
        if (st != IDL_STATUS_OK) {
            return st;
        }
        st = encode_type_ref(builder, builder->arena, buf, len, cap, f->type);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    return IDL_STATUS_OK;
}

/* Build FUNC type entry */
static idl_status build_func_type_entry(idl_type_table_builder *builder,
                                        idl_type               *type,
                                        uint8_t               **buf,
                                        size_t                 *len,
                                        size_t                 *cap) {
    for (size_t i = 0; i < type->data.func.args_len; i++) {
        idl_status st = idl_type_table_builder_build_type(
            builder, type->data.func.args[i], NULL);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    for (size_t i = 0; i < type->data.func.rets_len; i++) {
        idl_status st = idl_type_table_builder_build_type(
            builder, type->data.func.rets[i], NULL);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    idl_status st = write_sleb128(builder->arena, buf, len, cap, IDL_TYPE_FUNC);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    st = write_uleb128(builder->arena, buf, len, cap, type->data.func.args_len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    for (size_t i = 0; i < type->data.func.args_len; i++) {
        st = encode_type_ref(builder, builder->arena, buf, len, cap,
                             type->data.func.args[i]);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    st = write_uleb128(builder->arena, buf, len, cap, type->data.func.rets_len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    for (size_t i = 0; i < type->data.func.rets_len; i++) {
        st = encode_type_ref(builder, builder->arena, buf, len, cap,
                             type->data.func.rets[i]);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    st =
        write_uleb128(builder->arena, buf, len, cap, type->data.func.modes_len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    for (size_t i = 0; i < type->data.func.modes_len; i++) {
        st = write_sleb128(builder->arena, buf, len, cap,
                           type->data.func.modes[i]);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    return IDL_STATUS_OK;
}

/* Append string to buffer */
static idl_status append_string_to_buffer(idl_arena  *arena,
                                          uint8_t   **buf,
                                          size_t     *len,
                                          size_t     *cap,
                                          const char *str,
                                          size_t      str_len) {
    while (*len + str_len > *cap) {
        size_t   new_cap = (*cap == 0) ? 64 : (*cap * 2);
        uint8_t *new_buf = idl_arena_alloc(arena, new_cap);
        if (!new_buf) {
            return IDL_STATUS_ERR_ALLOC;
        }
        if (*buf && *cap > 0) {
            memcpy(new_buf, *buf, *len);
        }
        *buf = new_buf;
        *cap = new_cap;
    }
    memcpy(*buf + *len, str, str_len);
    *len += str_len;
    return IDL_STATUS_OK;
}

/* Build SERVICE type entry */
static idl_status build_service_type_entry(idl_type_table_builder *builder,
                                           idl_type               *type,
                                           uint8_t               **buf,
                                           size_t                 *len,
                                           size_t                 *cap) {
    for (size_t i = 0; i < type->data.service.methods_len; i++) {
        idl_status st = idl_type_table_builder_build_type(
            builder, type->data.service.methods[i].type, NULL);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    idl_status st =
        write_sleb128(builder->arena, buf, len, cap, IDL_TYPE_SERVICE);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    st = write_uleb128(builder->arena, buf, len, cap,
                       type->data.service.methods_len);
    if (st != IDL_STATUS_OK) {
        return st;
    }

    for (size_t i = 0; i < type->data.service.methods_len; i++) {
        idl_method *m = &type->data.service.methods[i];
        size_t      name_len = strlen(m->name);

        st = write_uleb128(builder->arena, buf, len, cap, name_len);
        if (st != IDL_STATUS_OK) {
            return st;
        }

        st = append_string_to_buffer(builder->arena, buf, len, cap, m->name,
                                     name_len);
        if (st != IDL_STATUS_OK) {
            return st;
        }

        st = encode_type_ref(builder, builder->arena, buf, len, cap, m->type);
        if (st != IDL_STATUS_OK) {
            return st;
        }
    }

    return IDL_STATUS_OK;
}

idl_status idl_type_table_builder_build_type(idl_type_table_builder *builder,
                                             idl_type               *type,
                                             int32_t *out_index) {
    if (!builder || !type) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    /* Check if already built */
    int32_t existing;
    if (find_type_index(builder, type, &existing)) {
        if (out_index)
            *out_index = existing;
        return IDL_STATUS_OK;
    }

    /* Resolve VAR types */
    idl_type *actual_type = type;
    if (type->kind == IDL_KIND_VAR && builder->env) {
        actual_type = idl_type_env_rec_find(builder->env, type->data.var_name);
        if (!actual_type) {
            return IDL_STATUS_ERR_INVALID_ARG;
        }
    }

    /* Primitive types don't go in the type table */
    if (idl_type_is_primitive(actual_type)) {
        if (out_index)
            *out_index = idl_type_opcode(actual_type->kind);
        return IDL_STATUS_OK;
    }

    /* Reserve a slot in the type table */
    int32_t idx = (int32_t)builder->entries_count;

    /* Add mapping before recursing (for recursive types) */
    idl_status st = add_type_mapping(builder, type, idx);
    if (st != IDL_STATUS_OK)
        return st;

    /* Grow entries array if needed */
    if (builder->entries_count >= builder->entries_capacity) {
        st = grow_array(builder->arena, (void **)&builder->entries,
                        &builder->entries_capacity, sizeof(uint8_t *));
        if (st != IDL_STATUS_OK)
            return st;

        size_t *new_lens = idl_arena_alloc(
            builder->arena, builder->entries_capacity * sizeof(size_t));
        if (!new_lens)
            return IDL_STATUS_ERR_ALLOC;
        if (builder->entry_lens) {
            memcpy(new_lens, builder->entry_lens,
                   (builder->entries_capacity / 2) * sizeof(size_t));
        }
        builder->entry_lens = new_lens;
    }

    builder->entries_count++;

    uint8_t *buf = NULL;
    size_t   len = 0, cap = 0;

    switch (actual_type->kind) {
    case IDL_KIND_OPT:
        st = build_opt_type_entry(builder, actual_type, &buf, &len, &cap);
        break;

    case IDL_KIND_VEC:
        st = build_vec_type_entry(builder, actual_type, &buf, &len, &cap);
        break;

    case IDL_KIND_RECORD:
    case IDL_KIND_VARIANT:
        st = build_record_variant_entry(builder, actual_type, &buf, &len, &cap);
        break;

    case IDL_KIND_FUNC:
        st = build_func_type_entry(builder, actual_type, &buf, &len, &cap);
        break;

    case IDL_KIND_SERVICE:
        st = build_service_type_entry(builder, actual_type, &buf, &len, &cap);
        break;

    default:
        return IDL_STATUS_ERR_UNSUPPORTED;
    }

    if (st != IDL_STATUS_OK) {
        return st;
    }

    builder->entries[idx] = buf;
    builder->entry_lens[idx] = len;

    if (out_index)
        *out_index = idx;

    return IDL_STATUS_OK;
}

idl_status idl_type_table_builder_push_arg(idl_type_table_builder *builder,
                                           idl_type               *type) {
    if (!builder || !type) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    /* Build the type (registers all nested composite types) */
    idl_status st = idl_type_table_builder_build_type(builder, type, NULL);
    if (st != IDL_STATUS_OK)
        return st;

    /* Add to args list */
    if (builder->args_count >= builder->args_capacity) {
        st = grow_array(builder->arena, (void **)&builder->args,
                        &builder->args_capacity, sizeof(idl_type *));
        if (st != IDL_STATUS_OK)
            return st;
    }

    builder->args[builder->args_count++] = type;
    return IDL_STATUS_OK;
}

idl_status idl_type_table_builder_serialize(idl_type_table_builder *builder,
                                            uint8_t               **out,
                                            size_t                 *out_len) {
    if (!builder || !out || !out_len) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    uint8_t   *buf = NULL;
    size_t     len = 0, cap = 0;
    idl_status st;

    /* Type table count */
    st =
        write_uleb128(builder->arena, &buf, &len, &cap, builder->entries_count);
    if (st != IDL_STATUS_OK)
        return st;

    /* Type table entries */
    for (size_t i = 0; i < builder->entries_count; i++) {
        while (len + builder->entry_lens[i] > cap) {
            size_t   new_cap = (cap == 0) ? 64 : (cap * 2);
            uint8_t *new_buf = idl_arena_alloc(builder->arena, new_cap);
            if (!new_buf)
                return IDL_STATUS_ERR_ALLOC;
            if (buf && cap > 0) {
                memcpy(new_buf, buf, len);
            }
            buf = new_buf;
            cap = new_cap;
        }
        memcpy(buf + len, builder->entries[i], builder->entry_lens[i]);
        len += builder->entry_lens[i];
    }

    /* Arg count */
    st = write_uleb128(builder->arena, &buf, &len, &cap, builder->args_count);
    if (st != IDL_STATUS_OK)
        return st;

    /* Arg type references */
    for (size_t i = 0; i < builder->args_count; i++) {
        st = encode_type_ref(builder, builder->arena, &buf, &len, &cap,
                             builder->args[i]);
        if (st != IDL_STATUS_OK)
            return st;
    }

    *out = buf;
    *out_len = len;
    return IDL_STATUS_OK;
}

idl_status
idl_type_table_builder_encode_type(const idl_type_table_builder *builder,
                                   const idl_type               *type,
                                   int32_t                      *out_index) {
    if (!builder || !type || !out_index) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    if (idl_type_is_primitive(type)) {
        *out_index = idl_type_opcode(type->kind);
        return IDL_STATUS_OK;
    }

    if (find_type_index(builder, type, out_index)) {
        return IDL_STATUS_OK;
    }

    return IDL_STATUS_ERR_INVALID_ARG;
}
