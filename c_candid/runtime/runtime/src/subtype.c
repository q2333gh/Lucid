/*
 * Candid Subtype Checking
 *
 * Implements subtype relationship checking between Candid types using
 * a memoization table (gamma) to avoid infinite recursion. Determines:
 * - Whether one type is a subtype of another (e.g., nat8 <: nat)
 * - Type compatibility for deserialization and coercion
 * - Recursive type handling with cycle detection
 *
 * Subtype checking is essential for Candid's flexible type system, allowing
 * values of more specific types to be used where more general types are
 * expected.
 */
#include "idl/subtype.h"
/* #include <stdio.h> */
#include <string.h>

#define GAMMA_BUCKET_COUNT 64

/* Simple hash for type pair */
static size_t hash_type_pair(const idl_type *t1, const idl_type *t2) {
    /* Use pointer addresses for hashing */
    uintptr_t h1 = (uintptr_t)t1;
    uintptr_t h2 = (uintptr_t)t2;
    return (size_t)((h1 * 31) ^ h2);
}

idl_status idl_gamma_init(idl_gamma *gamma, idl_arena *arena) {
    if (!gamma || !arena) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    gamma->arena = arena;
    gamma->bucket_count = GAMMA_BUCKET_COUNT;
    gamma->count = 0;
    gamma->buckets = idl_arena_alloc_zeroed(arena, sizeof(idl_gamma_entry *) *
                                                       gamma->bucket_count);

    if (!gamma->buckets) {
        return IDL_STATUS_ERR_ALLOC;
    }

    return IDL_STATUS_OK;
}

int idl_gamma_contains(const idl_gamma *gamma,
                       const idl_type  *t1,
                       const idl_type  *t2) {
    if (!gamma || !t1 || !t2) {
        return 0;
    }

    size_t bucket = hash_type_pair(t1, t2) % gamma->bucket_count;

    for (idl_gamma_entry *e = gamma->buckets[bucket]; e; e = e->next) {
        if (e->t1 == t1 && e->t2 == t2) {
            return 1;
        }
    }

    return 0;
}

int idl_gamma_insert(idl_gamma *gamma, const idl_type *t1, const idl_type *t2) {
    if (!gamma || !t1 || !t2) {
        return 0;
    }

    if (idl_gamma_contains(gamma, t1, t2)) {
        return 0; /* already present */
    }

    size_t bucket = hash_type_pair(t1, t2) % gamma->bucket_count;

    idl_gamma_entry *entry =
        idl_arena_alloc(gamma->arena, sizeof(idl_gamma_entry));
    if (!entry) {
        return 0;
    }

    entry->t1 = t1;
    entry->t2 = t2;
    entry->next = gamma->buckets[bucket];
    gamma->buckets[bucket] = entry;
    gamma->count++;

    return 1;
}

void idl_gamma_remove(idl_gamma      *gamma,
                      const idl_type *t1,
                      const idl_type *t2) {
    if (!gamma || !t1 || !t2) {
        return;
    }

    size_t bucket = hash_type_pair(t1, t2) % gamma->bucket_count;

    idl_gamma_entry **prev = &gamma->buckets[bucket];
    for (idl_gamma_entry *e = gamma->buckets[bucket]; e; e = e->next) {
        if (e->t1 == t1 && e->t2 == t2) {
            *prev = e->next;
            gamma->count--;
            return;
        }
        prev = &e->next;
    }
}

int idl_type_is_optional_like(const idl_type_env *env, const idl_type *type) {
    if (!type) {
        return 0;
    }

    const idl_type *t = type;
    if (t->kind == IDL_KIND_VAR && env) {
        t = idl_type_env_trace(env, type);
        if (!t) {
            return 0;
        }
    }

    switch (t->kind) {
    case IDL_KIND_NULL:
    case IDL_KIND_RESERVED:
    case IDL_KIND_OPT:
        return 1;
    default:
        return 0;
    }
}

/* Forward declaration for recursion */
static idl_subtype_result subtype_check_impl(idl_opt_report      report,
                                             idl_gamma          *gamma,
                                             const idl_type_env *env,
                                             const idl_type     *t1,
                                             const idl_type     *t2);

/* Resolve VAR/Knot types */
static const idl_type *resolve_type(const idl_type_env *env,
                                    const idl_type     *type) {
    if (!type) {
        return NULL;
    }
    if (type->kind == IDL_KIND_VAR && env) {
        return idl_type_env_trace(env, type);
    }
    return type;
}

/* Check base subtype rules */
static idl_subtype_result check_base_subtype_rules(const idl_type *rt1,
                                                   const idl_type *rt2) {
    if (rt2->kind == IDL_KIND_RESERVED) {
        return IDL_SUBTYPE_OK;
    }

    if (rt1->kind == IDL_KIND_EMPTY) {
        return IDL_SUBTYPE_OK;
    }

    if (rt1->kind == IDL_KIND_NAT && rt2->kind == IDL_KIND_INT) {
        return IDL_SUBTYPE_OK;
    }

    if (rt1->kind == rt2->kind && idl_type_is_primitive(rt1)) {
        return IDL_SUBTYPE_OK;
    }

    return IDL_SUBTYPE_FAIL;
}

/* Check vec subtype */
static idl_subtype_result check_vec_subtype(idl_opt_report      report,
                                            idl_gamma          *gamma,
                                            const idl_type_env *env,
                                            const idl_type     *rt1,
                                            const idl_type     *rt2) {
    if (rt1->kind == IDL_KIND_VEC && rt2->kind == IDL_KIND_VEC) {
        return subtype_check_impl(report, gamma, env, rt1->data.inner,
                                  rt2->data.inner);
    }
    return IDL_SUBTYPE_FAIL;
}

/* Check opt subtype - null case */
static idl_subtype_result check_opt_subtype_null(const idl_type *rt1,
                                                 const idl_type *rt2) {
    if (rt2->kind == IDL_KIND_OPT && rt1->kind == IDL_KIND_NULL) {
        return IDL_SUBTYPE_OK;
    }
    return IDL_SUBTYPE_FAIL;
}

/* Check opt subtype - opt T1 <: opt T2 */
static idl_subtype_result check_opt_subtype_opt(idl_opt_report      report,
                                                idl_gamma          *gamma,
                                                const idl_type_env *env,
                                                const idl_type     *rt1,
                                                const idl_type     *rt2) {
    if (rt1->kind == IDL_KIND_OPT && rt2->kind == IDL_KIND_OPT) {
        idl_subtype_result inner_res = subtype_check_impl(
            report, gamma, env, rt1->data.inner, rt2->data.inner);
        if (inner_res == IDL_SUBTYPE_OK) {
            return IDL_SUBTYPE_OK;
        }
    }
    return IDL_SUBTYPE_FAIL;
}

/* Check opt subtype - T <: opt T2 */
static idl_subtype_result check_opt_subtype_wrap(idl_opt_report      report,
                                                 idl_gamma          *gamma,
                                                 const idl_type_env *env,
                                                 const idl_type     *rt1,
                                                 const idl_type     *rt2) {
    const idl_type *inner_t2 = resolve_type(env, rt2->data.inner);
    if (inner_t2 && !idl_type_is_optional_like(env, inner_t2)) {
        idl_subtype_result inner_res =
            subtype_check_impl(report, gamma, env, rt1, rt2->data.inner);
        if (inner_res == IDL_SUBTYPE_OK) {
            return IDL_SUBTYPE_OK;
        }
    }
    return IDL_SUBTYPE_FAIL;
}

/* Check opt subtype - special rule */
static idl_subtype_result check_opt_subtype_special(idl_opt_report report) {
    switch (report) {
    case IDL_OPT_SILENCE:
        return IDL_SUBTYPE_OPT_SPECIAL;
    case IDL_OPT_WARNING:
        return IDL_SUBTYPE_OPT_SPECIAL;
    case IDL_OPT_ERROR:
        return IDL_SUBTYPE_FAIL;
    }
    return IDL_SUBTYPE_FAIL;
}

/* Find matching field in record */
static const idl_field *find_record_field(const idl_type *record_type,
                                          uint32_t        label_id) {
    for (size_t j = 0; j < record_type->data.record.fields_len; j++) {
        if (record_type->data.record.fields[j].label.id == label_id) {
            return &record_type->data.record.fields[j];
        }
    }
    return NULL;
}

/* Check record subtype */
static idl_subtype_result check_record_subtype(idl_opt_report      report,
                                               idl_gamma          *gamma,
                                               const idl_type_env *env,
                                               const idl_type     *rt1,
                                               const idl_type     *rt2) {
    for (size_t i = 0; i < rt2->data.record.fields_len; i++) {
        idl_field       *f2 = &rt2->data.record.fields[i];
        const idl_field *f1 = find_record_field(rt1, f2->label.id);

        if (f1) {
            idl_subtype_result field_res =
                subtype_check_impl(report, gamma, env, f1->type, f2->type);
            if (field_res == IDL_SUBTYPE_FAIL) {
                return IDL_SUBTYPE_FAIL;
            }
        } else {
            if (!idl_type_is_optional_like(env, f2->type)) {
                return IDL_SUBTYPE_FAIL;
            }
        }
    }

    return IDL_SUBTYPE_OK;
}

/* Check variant subtype */
static idl_subtype_result check_variant_subtype(idl_opt_report      report,
                                                idl_gamma          *gamma,
                                                const idl_type_env *env,
                                                const idl_type     *rt1,
                                                const idl_type     *rt2) {
    for (size_t i = 0; i < rt1->data.record.fields_len; i++) {
        idl_field       *f1 = &rt1->data.record.fields[i];
        const idl_field *f2 = find_record_field(rt2, f1->label.id);

        if (!f2) {
            return IDL_SUBTYPE_FAIL;
        }

        idl_subtype_result field_res =
            subtype_check_impl(report, gamma, env, f1->type, f2->type);
        if (field_res == IDL_SUBTYPE_FAIL) {
            return IDL_SUBTYPE_FAIL;
        }
    }

    return IDL_SUBTYPE_OK;
}

/* Check func subtype */
static idl_subtype_result check_func_subtype(idl_opt_report      report,
                                             idl_gamma          *gamma,
                                             const idl_type_env *env,
                                             const idl_type     *rt1,
                                             const idl_type     *rt2) {
    idl_func *f1 = &((idl_type *)rt1)->data.func;
    idl_func *f2 = &((idl_type *)rt2)->data.func;

    if (f1->args_len != f2->args_len) {
        return IDL_SUBTYPE_FAIL;
    }

    if (f1->rets_len != f2->rets_len) {
        return IDL_SUBTYPE_FAIL;
    }

    for (size_t i = 0; i < f1->args_len; i++) {
        idl_subtype_result res =
            subtype_check_impl(report, gamma, env, f2->args[i], f1->args[i]);
        if (res == IDL_SUBTYPE_FAIL) {
            return IDL_SUBTYPE_FAIL;
        }
    }

    for (size_t i = 0; i < f1->rets_len; i++) {
        idl_subtype_result res =
            subtype_check_impl(report, gamma, env, f1->rets[i], f2->rets[i]);
        if (res == IDL_SUBTYPE_FAIL) {
            return IDL_SUBTYPE_FAIL;
        }
    }

    if (f1->modes_len != f2->modes_len) {
        return IDL_SUBTYPE_FAIL;
    }
    for (size_t i = 0; i < f1->modes_len; i++) {
        if (f1->modes[i] != f2->modes[i]) {
            return IDL_SUBTYPE_FAIL;
        }
    }

    return IDL_SUBTYPE_OK;
}

/* Find matching method in service */
static const idl_method *find_service_method(const idl_service *service,
                                             const char        *name) {
    for (size_t j = 0; j < service->methods_len; j++) {
        if (strcmp(service->methods[j].name, name) == 0) {
            return &service->methods[j];
        }
    }
    return NULL;
}

/* Check service subtype */
static idl_subtype_result check_service_subtype(idl_opt_report      report,
                                                idl_gamma          *gamma,
                                                const idl_type_env *env,
                                                const idl_type     *rt1,
                                                const idl_type     *rt2) {
    idl_service *s1 = &((idl_type *)rt1)->data.service;
    idl_service *s2 = &((idl_type *)rt2)->data.service;

    for (size_t i = 0; i < s2->methods_len; i++) {
        idl_method       *m2 = &s2->methods[i];
        const idl_method *m1 = find_service_method(s1, m2->name);

        if (!m1) {
            return IDL_SUBTYPE_FAIL;
        }

        idl_subtype_result res =
            subtype_check_impl(report, gamma, env, m1->type, m2->type);
        if (res == IDL_SUBTYPE_FAIL) {
            return IDL_SUBTYPE_FAIL;
        }
    }

    return IDL_SUBTYPE_OK;
}

/* Handle VAR types with Gamma cache, coinductive rule */
static int subtype_check_var_case(idl_opt_report      report,
                                  idl_gamma          *gamma,
                                  const idl_type_env *env,
                                  const idl_type     *t1,
                                  const idl_type     *t2,
                                  idl_subtype_result *out_res) {
    if (t1->kind != IDL_KIND_VAR && t2->kind != IDL_KIND_VAR) {
        return 0;
    }

    if (!idl_gamma_insert(gamma, t1, t2)) {
        *out_res = IDL_SUBTYPE_OK;
        return 1;
    }

    const idl_type *resolved_t1 = resolve_type(env, t1);
    const idl_type *resolved_t2 = resolve_type(env, t2);

    if (!resolved_t1 || !resolved_t2) {
        idl_gamma_remove(gamma, t1, t2);
        *out_res = IDL_SUBTYPE_FAIL;
        return 1;
    }

    idl_subtype_result res =
        subtype_check_impl(report, gamma, env, resolved_t1, resolved_t2);

    if (res == IDL_SUBTYPE_FAIL) {
        idl_gamma_remove(gamma, t1, t2);
    }

    *out_res = res;
    return 1;
}

static idl_subtype_result subtype_check_impl(idl_opt_report      report,
                                             idl_gamma          *gamma,
                                             const idl_type_env *env,
                                             const idl_type     *t1,
                                             const idl_type     *t2) {
    if (t1 == t2) {
        return IDL_SUBTYPE_OK;
    }

    idl_subtype_result var_res;
    if (subtype_check_var_case(report, gamma, env, t1, t2, &var_res)) {
        return var_res;
    }

    const idl_type *rt1 = resolve_type(env, t1);
    const idl_type *rt2 = resolve_type(env, t2);

    if (!rt1 || !rt2) {
        return IDL_SUBTYPE_FAIL;
    }

    idl_subtype_result res = check_base_subtype_rules(rt1, rt2);
    if (res != IDL_SUBTYPE_FAIL) {
        return res;
    }

    res = check_vec_subtype(report, gamma, env, rt1, rt2);
    if (res != IDL_SUBTYPE_FAIL) {
        return res;
    }

    if (rt2->kind == IDL_KIND_OPT) {
        res = check_opt_subtype_null(rt1, rt2);
        if (res == IDL_SUBTYPE_OK) {
            return res;
        }

        res = check_opt_subtype_opt(report, gamma, env, rt1, rt2);
        if (res == IDL_SUBTYPE_OK) {
            return res;
        }

        res = check_opt_subtype_wrap(report, gamma, env, rt1, rt2);
        if (res == IDL_SUBTYPE_OK) {
            return res;
        }

        return check_opt_subtype_special(report);
    }

    if (rt1->kind == IDL_KIND_RECORD && rt2->kind == IDL_KIND_RECORD) {
        return check_record_subtype(report, gamma, env, rt1, rt2);
    }

    if (rt1->kind == IDL_KIND_VARIANT && rt2->kind == IDL_KIND_VARIANT) {
        return check_variant_subtype(report, gamma, env, rt1, rt2);
    }

    if (rt1->kind == IDL_KIND_FUNC && rt2->kind == IDL_KIND_FUNC) {
        return check_func_subtype(report, gamma, env, rt1, rt2);
    }

    if (rt1->kind == IDL_KIND_SERVICE && rt2->kind == IDL_KIND_SERVICE) {
        return check_service_subtype(report, gamma, env, rt1, rt2);
    }

    return IDL_SUBTYPE_FAIL;
}

idl_subtype_result idl_subtype_check(idl_opt_report      report,
                                     idl_gamma          *gamma,
                                     const idl_type_env *env,
                                     const idl_type     *t1,
                                     const idl_type     *t2) {
    if (!gamma || !t1 || !t2) {
        return IDL_SUBTYPE_FAIL;
    }

    return subtype_check_impl(report, gamma, env, t1, t2);
}

idl_subtype_result idl_subtype(const idl_type_env *env,
                               const idl_type     *t1,
                               const idl_type     *t2,
                               idl_arena          *arena) {
    if (!t1 || !t2 || !arena) {
        return IDL_SUBTYPE_FAIL;
    }

    idl_gamma gamma;
    if (idl_gamma_init(&gamma, arena) != IDL_STATUS_OK) {
        return IDL_SUBTYPE_FAIL;
    }

    return idl_subtype_check(IDL_OPT_WARNING, &gamma, env, t1, t2);
}
