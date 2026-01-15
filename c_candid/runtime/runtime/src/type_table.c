/*
 * Candid Type Table Builder
 *
 * Builds and serializes the type table portion of Candid headers. Features:
 * - Type deduplication: Reuses type table entries for identical types
 * - Recursive type registration: Handles nested composite types
 * - Type table serialization: Converts type table to wire format
 * - Type environment integration: Tracks type definitions and references
 *
 * The type table allows Candid to efficiently represent complex types by
 * defining them once and referencing them by index, reducing message size
 * for repeated or nested types.
 */
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
static int labels_equal(const idl_label *a, const idl_label *b) {
    if (!a || !b) {
        return 0;
    }
    if (a->kind != b->kind || a->id != b->id) {
        return 0;
    }
    if (a->kind == IDL_LABEL_NAME && a->name && b->name) {
        return strcmp(a->name, b->name) == 0;
    }
    return 1;
}

static int
type_equal_internal(const idl_type *a, const idl_type *b, size_t depth) {
    if (a == b) {
        return 1;
    }
    if (!a || !b) {
        return 0;
    }
    if (depth > 64) {
        return 0;
    }
    if (a->kind != b->kind) {
        return 0;
    }

    switch (a->kind) {
    case IDL_KIND_NULL:
    case IDL_KIND_BOOL:
    case IDL_KIND_NAT:
    case IDL_KIND_INT:
    case IDL_KIND_NAT8:
    case IDL_KIND_NAT16:
    case IDL_KIND_NAT32:
    case IDL_KIND_NAT64:
    case IDL_KIND_INT8:
    case IDL_KIND_INT16:
    case IDL_KIND_INT32:
    case IDL_KIND_INT64:
    case IDL_KIND_FLOAT32:
    case IDL_KIND_FLOAT64:
    case IDL_KIND_TEXT:
    case IDL_KIND_RESERVED:
    case IDL_KIND_EMPTY:
    case IDL_KIND_PRINCIPAL:
        return 1;
    case IDL_KIND_OPT:
    case IDL_KIND_VEC:
        return type_equal_internal(a->data.inner, b->data.inner, depth + 1);
    case IDL_KIND_RECORD:
    case IDL_KIND_VARIANT: {
        if (a->data.record.fields_len != b->data.record.fields_len) {
            return 0;
        }
        for (size_t i = 0; i < a->data.record.fields_len; i++) {
            const idl_field *af = &a->data.record.fields[i];
            const idl_field *bf = &b->data.record.fields[i];
            if (!labels_equal(&af->label, &bf->label)) {
                return 0;
            }
            if (!type_equal_internal(af->type, bf->type, depth + 1)) {
                return 0;
            }
        }
        return 1;
    }
    case IDL_KIND_FUNC: {
        if (a->data.func.args_len != b->data.func.args_len ||
            a->data.func.rets_len != b->data.func.rets_len ||
            a->data.func.modes_len != b->data.func.modes_len) {
            return 0;
        }
        for (size_t i = 0; i < a->data.func.args_len; i++) {
            if (!type_equal_internal(a->data.func.args[i], b->data.func.args[i],
                                     depth + 1)) {
                return 0;
            }
        }
        for (size_t i = 0; i < a->data.func.rets_len; i++) {
            if (!type_equal_internal(a->data.func.rets[i], b->data.func.rets[i],
                                     depth + 1)) {
                return 0;
            }
        }
        for (size_t i = 0; i < a->data.func.modes_len; i++) {
            if (a->data.func.modes[i] != b->data.func.modes[i]) {
                return 0;
            }
        }
        return 1;
    }
    case IDL_KIND_SERVICE: {
        if (a->data.service.methods_len != b->data.service.methods_len) {
            return 0;
        }
        for (size_t i = 0; i < a->data.service.methods_len; i++) {
            const idl_method *am = &a->data.service.methods[i];
            const idl_method *bm = &b->data.service.methods[i];
            if (!am->name || !bm->name || strcmp(am->name, bm->name) != 0) {
                return 0;
            }
            if (!type_equal_internal(am->type, bm->type, depth + 1)) {
                return 0;
            }
        }
        return 1;
    }
    case IDL_KIND_VAR:
        if (!a->data.var_name || !b->data.var_name) {
            return 0;
        }
        return strcmp(a->data.var_name, b->data.var_name) == 0;
    default:
        return 0;
    }
}

static int type_equal(const idl_type *a, const idl_type *b) {
    return type_equal_internal(a, b, 0);
}

static int opt_vec_primitive_equal(const idl_type *a, const idl_type *b) {
    if (!a || !b || a->kind != b->kind) {
        return 0;
    }
    if (a->kind != IDL_KIND_OPT && a->kind != IDL_KIND_VEC) {
        return 0;
    }
    if (!a->data.inner || !b->data.inner) {
        return 0;
    }
    if (!idl_type_is_primitive(a->data.inner) ||
        !idl_type_is_primitive(b->data.inner)) {
        return 0;
    }
    return a->data.inner->kind == b->data.inner->kind;
}

static int find_type_index(const idl_type_table_builder *builder,
                           const idl_type               *type,
                           int32_t                      *out) {
    for (size_t i = 0; i < builder->type_map_count; i++) {
        if (builder->type_keys[i] == type ||
            opt_vec_primitive_equal(builder->type_keys[i], type) ||
            type_equal(builder->type_keys[i], type)) {
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

    /* Deduplicate opt/vec of primitive inner types using resolved type. */
    if ((actual_type->kind == IDL_KIND_OPT ||
         actual_type->kind == IDL_KIND_VEC) &&
        actual_type->data.inner &&
        idl_type_is_primitive(actual_type->data.inner)) {
        for (size_t i = 0; i < builder->type_map_count; i++) {
            idl_type *candidate = builder->type_keys[i];
            if (!candidate) {
                continue;
            }
            if (candidate->kind == IDL_KIND_VAR && builder->env) {
                candidate = idl_type_env_rec_find(builder->env,
                                                  candidate->data.var_name);
                if (!candidate) {
                    continue;
                }
            }
            if (candidate->kind == actual_type->kind && candidate->data.inner &&
                idl_type_is_primitive(candidate->data.inner) &&
                candidate->data.inner->kind == actual_type->data.inner->kind) {
                if (out_index) {
                    *out_index = builder->type_indices[i];
                }
                return IDL_STATUS_OK;
            }
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
