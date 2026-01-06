/**
 * ic_candid_builder.h - Simplified Candid Builder API for C
 *
 * This header provides more concise syntax for building Candid types and values
 * using C99 compound literals and variadic macros to reduce boilerplate.
 *
 * Key improvements over raw IDL API:
 * 1. Eliminates manual index management
 * 2. Automatic field count calculation
 * 3. Single-expression type/value construction
 * 4. Less repetitive field declarations
 *
 * Usage:
 *   #include "ic_candid_builder.h"
 *
 *   // Build type with compact syntax
 *   idl_type *addr_type = IDL_RECORD(arena,
 *       IDL_FIELD("street", idl_type_text(arena)),
 *       IDL_FIELD("city",   idl_type_text(arena)),
 *       IDL_FIELD("zip",    idl_type_nat(arena))
 *   );
 *
 *   // Build value with compact syntax
 *   idl_value *addr_val = IDL_RECORD_VALUE(arena,
 *       IDL_VALUE_FIELD("street", idl_value_text_cstr(arena, "123 Main St")),
 *       IDL_VALUE_FIELD("city",   idl_value_text_cstr(arena, "San Francisco")),
 *       IDL_VALUE_FIELD("zip",    idl_value_nat32(arena, 94102))
 *   );
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "idl/candid.h"
#include "idl/hash.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 * Field Sorting Helpers
 * =============================================================================
 *
 * Candid requires record and variant fields to be sorted by their hash values.
 * These inline functions provide this functionality.
 */

static inline void idl_fields_sort_inplace(idl_field *fields, size_t len) {
    if (fields == NULL || len < 2) {
        return;
    }

    // Stable insertion sort (len is typically small for Candid records)
    for (size_t i = 1; i < len; ++i) {
        idl_field key = fields[i];
        uint32_t  key_id = key.label.id;
        size_t    j = i;
        while (j > 0 && fields[j - 1].label.id > key_id) {
            fields[j] = fields[j - 1];
            --j;
        }
        fields[j] = key;
    }
}

static inline void idl_value_fields_sort_inplace(idl_value_field *fields,
                                                 size_t           len) {
    if (fields == NULL || len < 2) {
        return;
    }

    // Stable insertion sort
    for (size_t i = 1; i < len; ++i) {
        idl_value_field key = fields[i];
        uint32_t        key_id = key.label.id;
        size_t          j = i;
        while (j > 0 && fields[j - 1].label.id > key_id) {
            fields[j] = fields[j - 1];
            --j;
        }
        fields[j] = key;
    }
}

/* ============================================================
 * SOLUTION 1: Macro-based API with Compound Literals
 *
 * Pros:
 * - Pure C99, no external dependencies
 * - Zero runtime overhead (compile-time expansion)
 * - Eliminates index management and magic numbers
 * - Reduces code by ~50%
 *
 * Cons:
 * - Macro debugging can be challenging
 * - Compound literal lifetime requires care
 * - Limited type safety (macro expansion)
 * ============================================================ */

/**
 * IDL_FIELD - Create a field for type definitions
 * Usage: IDL_FIELD("fieldname", type_expr)
 */
#define IDL_FIELD(name, type_expr)                                             \
    { .label = idl_label_name(name), .type = (type_expr) }

/**
 * IDL_VALUE_FIELD - Create a field for value definitions
 * Usage: IDL_VALUE_FIELD("fieldname", value_expr)
 */
#define IDL_VALUE_FIELD(name, value_expr)                                      \
    { .label = idl_label_name(name), .value = (value_expr) }

/**
 * Internal helper to count variadic arguments
 */
#define IDL_COUNT_ARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, \
                            _13, _14, _15, _16, _17, _18, _19, _20, N, ...)    \
    N
#define IDL_COUNT_ARGS(...)                                                    \
    IDL_COUNT_ARGS_IMPL(__VA_ARGS__, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11,   \
                        10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

// Helper function for sorted record creation
static inline idl_type *
idl_record_sorted(idl_arena *arena, idl_field *fields, size_t count) {
    if (!fields || count == 0) {
        return idl_type_record(arena, NULL, 0);
    }

    // CRITICAL: Copy fields to arena to ensure they persist beyond compound
    // literal lifetime Compound literals like (idl_field[]){...} have automatic
    // storage duration
    idl_field *arena_fields = idl_arena_alloc(arena, count * sizeof(idl_field));
    if (!arena_fields) {
        return NULL;
    }

    // Use memcpy for reliable copy
    memcpy(arena_fields, fields, count * sizeof(idl_field));

    // Sort fields by hash before creating record
    idl_fields_sort_inplace(arena_fields, count);

    return idl_type_record(arena, arena_fields, count);
}

/**
 * IDL_RECORD - Create a record type with automatic field counting and sorting
 * Usage: IDL_RECORD(arena, field1, field2, ...)
 *
 * Example:
 *   idl_type *address = IDL_RECORD(arena,
 *       IDL_FIELD("street", idl_type_text(arena)),
 *       IDL_FIELD("city",   idl_type_text(arena)),
 *       IDL_FIELD("zip",    idl_type_nat(arena))
 *   );
 *
 * Note: Uses GNU statement expression to ensure proper lifetime of compound
 * literal
 */
#define IDL_RECORD(arena, ...)                                                 \
    ({                                                                         \
        idl_field _idl_tmp_fields[] = {__VA_ARGS__};                           \
        idl_record_sorted((arena), _idl_tmp_fields,                            \
                          sizeof(_idl_tmp_fields) / sizeof(idl_field));        \
    })

// Helper function for sorted variant creation
static inline idl_type *
idl_variant_sorted(idl_arena *arena, idl_field *fields, size_t count) {
    if (!fields || count == 0) {
        return idl_type_variant(arena, NULL, 0);
    }

    // CRITICAL: Copy fields to arena to ensure they persist beyond compound
    // literal lifetime
    idl_field *arena_fields = idl_arena_alloc(arena, count * sizeof(idl_field));
    if (!arena_fields) {
        return NULL;
    }

    // Use memcpy for reliable copy
    memcpy(arena_fields, fields, count * sizeof(idl_field));

    // Sort fields by hash before creating variant
    idl_fields_sort_inplace(arena_fields, count);

    return idl_type_variant(arena, arena_fields, count);
}

/**
 * IDL_VARIANT - Create a variant type with automatic field counting and sorting
 * Usage: IDL_VARIANT(arena, field1, field2, ...)
 *
 * Note: Uses GNU statement expression to ensure proper lifetime of compound
 * literal
 */
#define IDL_VARIANT(arena, ...)                                                \
    ({                                                                         \
        idl_field _idl_tmp_fields[] = {__VA_ARGS__};                           \
        idl_variant_sorted((arena), _idl_tmp_fields,                           \
                           sizeof(_idl_tmp_fields) / sizeof(idl_field));       \
    })

// Helper function for sorted record value creation
static inline idl_value *idl_record_value_sorted(idl_arena       *arena,
                                                 idl_value_field *fields,
                                                 size_t           count) {
    if (!fields || count == 0) {
        return idl_value_record(arena, NULL, 0);
    }

    // CRITICAL: Must copy to arena before sorting because compound literal
    // has automatic storage duration. Even though idl_value_record() will
    // copy again, we need a stable copy for sorting.
    idl_value_field *arena_fields =
        idl_arena_alloc(arena, count * sizeof(idl_value_field));
    if (!arena_fields) {
        return NULL;
    }

    // Use memcpy for reliable copy
    memcpy(arena_fields, fields, count * sizeof(idl_value_field));

    // Sort fields by hash before creating record value
    idl_value_fields_sort_inplace(arena_fields, count);

    // idl_value_record will make another copy, but that's OK - it ensures
    // proper lifetime
    return idl_value_record(arena, arena_fields, count);
}

/**
 * IDL_RECORD_VALUE - Create a record value with automatic field counting and
 * sorting Usage: IDL_RECORD_VALUE(arena, value_field1, value_field2, ...)
 *
 * Example:
 *   idl_value *addr = IDL_RECORD_VALUE(arena,
 *       IDL_VALUE_FIELD("street", idl_value_text_cstr(arena, "123 Main St")),
 *       IDL_VALUE_FIELD("city",   idl_value_text_cstr(arena, "San Francisco")),
 *       IDL_VALUE_FIELD("zip",    idl_value_nat32(arena, 94102))
 *   );
 *
 * Note: Uses GNU statement expression to ensure proper lifetime of compound
 * literal
 */
#define IDL_RECORD_VALUE(arena, ...)                                           \
    ({                                                                         \
        idl_value_field _idl_tmp_vfields[] = {__VA_ARGS__};                    \
        idl_record_value_sorted((arena), _idl_tmp_vfields,                     \
                                sizeof(_idl_tmp_vfields) /                     \
                                    sizeof(idl_value_field));                  \
    })

/**
 * IDL_VEC - Create a vector value with automatic item counting
 * Usage: IDL_VEC(arena, item1, item2, ...)
 *
 * Example:
 *   idl_value *emails = IDL_VEC(arena,
 *       idl_value_text_cstr(arena, "dev@example.com"),
 *       idl_value_text_cstr(arena, "contact@example.com")
 *   );
 */
#define IDL_VEC(arena, ...)                                                    \
    idl_value_vec((arena), (idl_value *[]){__VA_ARGS__},                       \
                  IDL_COUNT_ARGS(__VA_ARGS__))

/* ============================================================
 * SOLUTION 2: Type-Value Unified Builders
 *
 * For cases where you want to define both type and value together,
 * these helper functions reduce duplication.
 * ============================================================ */

/**
 * Record builder state for chained construction
 */
typedef struct idl_record_builder {
    idl_arena       *arena;
    idl_field       *type_fields;
    idl_value_field *value_fields;
    size_t           capacity;
    size_t           count;
} idl_record_builder;

/**
 * Initialize a record builder
 * max_fields: maximum number of fields (allocated upfront)
 */
static inline idl_record_builder *idl_record_builder_new(idl_arena *arena,
                                                         size_t max_fields) {
    idl_record_builder *rb = idl_arena_alloc(arena, sizeof(idl_record_builder));
    if (!rb)
        return NULL;

    rb->arena = arena;
    rb->capacity = max_fields;
    rb->count = 0;
    rb->type_fields = idl_arena_alloc(arena, max_fields * sizeof(idl_field));
    rb->value_fields =
        idl_arena_alloc(arena, max_fields * sizeof(idl_value_field));

    if (!rb->type_fields || !rb->value_fields) {
        return NULL;
    }

    return rb;
}

/**
 * Add a text field to record builder
 */
static inline idl_record_builder *idl_record_builder_text(
    idl_record_builder *rb, const char *name, const char *value) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_text(rb->arena);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_text_cstr(rb->arena, value);

    rb->count++;
    return rb;
}

/**
 * Add a nat32 field to record builder
 */
static inline idl_record_builder *idl_record_builder_nat32(
    idl_record_builder *rb, const char *name, uint32_t value) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_nat(rb->arena);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_nat32(rb->arena, value);

    rb->count++;
    return rb;
}

/**
 * Add a nat64 field to record builder
 */
static inline idl_record_builder *idl_record_builder_nat64(
    idl_record_builder *rb, const char *name, uint64_t value) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_nat(rb->arena);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_nat64(rb->arena, value);

    rb->count++;
    return rb;
}

/**
 * Add a bool field to record builder
 */
static inline idl_record_builder *
idl_record_builder_bool(idl_record_builder *rb, const char *name, bool value) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_bool(rb->arena);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_bool(rb->arena, value);

    rb->count++;
    return rb;
}

/**
 * Add an int32 field to record builder
 */
static inline idl_record_builder *idl_record_builder_int32(
    idl_record_builder *rb, const char *name, int32_t value) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_int(rb->arena);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_int32(rb->arena, value);

    rb->count++;
    return rb;
}

/**
 * Add an int64 field to record builder
 */
static inline idl_record_builder *idl_record_builder_int64(
    idl_record_builder *rb, const char *name, int64_t value) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_int(rb->arena);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_int64(rb->arena, value);

    rb->count++;
    return rb;
}

/**
 * Add a float32 field to record builder
 */
static inline idl_record_builder *idl_record_builder_float32(
    idl_record_builder *rb, const char *name, float value) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_float32(rb->arena);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_float32(rb->arena, value);

    rb->count++;
    return rb;
}

/**
 * Add a float64 field to record builder
 */
static inline idl_record_builder *idl_record_builder_float64(
    idl_record_builder *rb, const char *name, double value) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_float64(rb->arena);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_float64(rb->arena, value);

    rb->count++;
    return rb;
}

/**
 * Add a principal field to record builder
 */
static inline idl_record_builder *idl_record_builder_principal(
    idl_record_builder *rb, const char *name, const uint8_t *data, size_t len) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_principal(rb->arena);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_principal(rb->arena, data, len);

    rb->count++;
    return rb;
}

/**
 * Add a blob field to record builder
 */
static inline idl_record_builder *idl_record_builder_blob(
    idl_record_builder *rb, const char *name, const uint8_t *data, size_t len) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_vec(rb->arena, idl_type_nat8(rb->arena));

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_blob(rb->arena, data, len);

    rb->count++;
    return rb;
}

/**
 * Add an optional field to record builder
 */
static inline idl_record_builder *
idl_record_builder_opt(idl_record_builder *rb,
                       const char         *name,
                       idl_type           *inner_type,
                       idl_value          *inner_value) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_opt(rb->arena, inner_type);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = inner_value
                                    ? idl_value_opt_some(rb->arena, inner_value)
                                    : idl_value_opt_none(rb->arena);

    rb->count++;
    return rb;
}

/**
 * Add a vec field to record builder
 */
static inline idl_record_builder *idl_record_builder_vec(idl_record_builder *rb,
                                                         const char *name,
                                                         idl_type   *elem_type,
                                                         idl_value **items,
                                                         size_t items_len) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = idl_type_vec(rb->arena, elem_type);

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = idl_value_vec(rb->arena, items, items_len);

    rb->count++;
    return rb;
}

/**
 * Add a custom field to record builder (for nested records, variants, etc.)
 */
static inline idl_record_builder *
idl_record_builder_field(idl_record_builder *rb,
                         const char         *name,
                         idl_type           *type,
                         idl_value          *value) {
    if (!rb || rb->count >= rb->capacity)
        return rb;

    size_t i = rb->count;
    rb->type_fields[i].label = idl_label_name(name);
    rb->type_fields[i].type = type;

    rb->value_fields[i].label = idl_label_name(name);
    rb->value_fields[i].value = value;

    rb->count++;
    return rb;
}

/**
 * Build and return the type
 */
static inline idl_type *idl_record_builder_build_type(idl_record_builder *rb) {
    if (!rb)
        return NULL;
    // Sort fields by hash before building
    idl_fields_sort_inplace(rb->type_fields, rb->count);
    return idl_type_record(rb->arena, rb->type_fields, rb->count);
}

/**
 * Build and return the value
 */
static inline idl_value *
idl_record_builder_build_value(idl_record_builder *rb) {
    if (!rb)
        return NULL;
    // Sort fields by hash before building
    idl_value_fields_sort_inplace(rb->value_fields, rb->count);
    return idl_value_record(rb->arena, rb->value_fields, rb->count);
}

/* ============================================================
 * SOLUTION 3: High-Level Type Shorthand Macros
 *
 * Pre-defined type constructors that include arena automatically
 * when used within IC_API_BUILDER_BEGIN/END blocks
 * ============================================================ */

/**
 * These macros assume 'arena' variable is in scope
 * (which is true within IC_API_BUILDER_BEGIN blocks)
 */
#define T_NULL idl_type_null(arena)
#define T_BOOL idl_type_bool(arena)
#define T_NAT idl_type_nat(arena)
#define T_INT idl_type_int(arena)
#define T_NAT8 idl_type_nat8(arena)
#define T_NAT16 idl_type_nat16(arena)
#define T_NAT32 idl_type_nat32(arena)
#define T_NAT64 idl_type_nat64(arena)
#define T_INT8 idl_type_int8(arena)
#define T_INT16 idl_type_int16(arena)
#define T_INT32 idl_type_int32(arena)
#define T_INT64 idl_type_int64(arena)
#define T_FLOAT32 idl_type_float32(arena)
#define T_FLOAT64 idl_type_float64(arena)
#define T_TEXT idl_type_text(arena)
#define T_PRINCIPAL idl_type_principal(arena)
#define T_OPT(t) idl_type_opt(arena, (t))
#define T_VEC(t) idl_type_vec(arena, (t))

/**
 * Value construction shorthands (also assume 'arena' in scope)
 */
#define V_NULL idl_value_null(arena)
#define V_BOOL(v) idl_value_bool(arena, (v))
#define V_NAT8(v) idl_value_nat8(arena, (v))
#define V_NAT16(v) idl_value_nat16(arena, (v))
#define V_NAT32(v) idl_value_nat32(arena, (v))
#define V_NAT64(v) idl_value_nat64(arena, (v))
#define V_INT8(v) idl_value_int8(arena, (v))
#define V_INT16(v) idl_value_int16(arena, (v))
#define V_INT32(v) idl_value_int32(arena, (v))
#define V_INT64(v) idl_value_int64(arena, (v))
#define V_FLOAT32(v) idl_value_float32(arena, (v))
#define V_FLOAT64(v) idl_value_float64(arena, (v))
#define V_TEXT(s) idl_value_text_cstr(arena, (s))
#define V_SOME(v) idl_value_opt_some(arena, (v))
#define V_NONE idl_value_opt_none(arena)

/**
 * Helper: Infer idl_type from idl_value
 * This is a simple implementation for common cases
 */
static inline idl_type *idl_type_from_value(idl_arena       *arena,
                                            const idl_value *value) {
    if (!value) {
        return NULL;
    }

    switch (value->kind) {
    case IDL_VALUE_NULL:
        return idl_type_null(arena);
    case IDL_VALUE_BOOL:
        return idl_type_bool(arena);
    case IDL_VALUE_NAT:
        return idl_type_nat(arena);
    case IDL_VALUE_NAT8:
        return idl_type_nat8(arena);
    case IDL_VALUE_NAT16:
        return idl_type_nat16(arena);
    case IDL_VALUE_NAT32:
        return idl_type_nat32(arena);
    case IDL_VALUE_NAT64:
        return idl_type_nat64(arena);
    case IDL_VALUE_INT:
        return idl_type_int(arena);
    case IDL_VALUE_INT8:
        return idl_type_int8(arena);
    case IDL_VALUE_INT16:
        return idl_type_int16(arena);
    case IDL_VALUE_INT32:
        return idl_type_int32(arena);
    case IDL_VALUE_INT64:
        return idl_type_int64(arena);
    case IDL_VALUE_FLOAT32:
        return idl_type_float32(arena);
    case IDL_VALUE_FLOAT64:
        return idl_type_float64(arena);
    case IDL_VALUE_TEXT:
        return idl_type_text(arena);
    case IDL_VALUE_PRINCIPAL:
        return idl_type_principal(arena);
    case IDL_VALUE_RECORD: {
        // Build record type from fields
        if (value->data.record.len == 0) {
            return idl_type_record(arena, NULL, 0);
        }

        idl_field *fields = (idl_field *)idl_arena_alloc(
            arena, value->data.record.len * sizeof(idl_field));
        if (!fields) {
            return NULL;
        }

        for (size_t i = 0; i < value->data.record.len; i++) {
            fields[i].label = value->data.record.fields[i].label;
            fields[i].type =
                idl_type_from_value(arena, value->data.record.fields[i].value);
            if (!fields[i].type) {
                return NULL;
            }
        }

        return idl_type_record(arena, fields, value->data.record.len);
    }
    case IDL_VALUE_VARIANT: {
        // For variant: build type with just the active field
        // Note: This is simplified - full type would need all variants
        idl_value_field *active_field =
            &value->data.record.fields[value->data.record.variant_index];
        idl_field variant_field = {
            .label = active_field->label,
            .type = idl_type_from_value(arena, active_field->value)};
        return idl_type_variant(arena, &variant_field, 1);
    }
    case IDL_VALUE_OPT: {
        if (value->data.opt) {
            idl_type *inner = idl_type_from_value(arena, value->data.opt);
            return idl_type_opt(arena, inner);
        } else {
            // None - can't infer inner type, default to null
            return idl_type_opt(arena, idl_type_null(arena));
        }
    }
    case IDL_VALUE_VEC: {
        if (value->data.vec.len > 0) {
            idl_type *elem =
                idl_type_from_value(arena, value->data.vec.items[0]);
            return idl_type_vec(arena, elem);
        } else {
            // Empty vec - can't infer element type, default to null
            return idl_type_vec(arena, idl_type_null(arena));
        }
    }
    default:
        return NULL;
    }
}

#ifdef __cplusplus
}
#endif
