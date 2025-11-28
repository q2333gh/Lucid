#include "idl/subtype.h"
#include <stdio.h>
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

int idl_gamma_contains(const idl_gamma *gamma, const idl_type *t1,
                       const idl_type *t2) {
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

void idl_gamma_remove(idl_gamma *gamma, const idl_type *t1,
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
static idl_subtype_result subtype_check_impl(idl_opt_report report,
                                             idl_gamma *gamma,
                                             const idl_type_env *env,
                                             const idl_type *t1,
                                             const idl_type *t2);

/* Resolve VAR/Knot types */
static const idl_type *resolve_type(const idl_type_env *env,
                                    const idl_type *type) {
  if (!type) {
    return NULL;
  }
  if (type->kind == IDL_KIND_VAR && env) {
    return idl_type_env_trace(env, type);
  }
  return type;
}

static idl_subtype_result subtype_check_impl(idl_opt_report report,
                                             idl_gamma *gamma,
                                             const idl_type_env *env,
                                             const idl_type *t1,
                                             const idl_type *t2) {
  /* Same type is always a subtype */
  if (t1 == t2) {
    return IDL_SUBTYPE_OK;
  }

  /* Handle VAR types with Gamma cache */
  if (t1->kind == IDL_KIND_VAR || t2->kind == IDL_KIND_VAR) {
    if (!idl_gamma_insert(gamma, t1, t2)) {
      /* Already in cache, assume OK (coinductive) */
      return IDL_SUBTYPE_OK;
    }

    const idl_type *resolved_t1 = resolve_type(env, t1);
    const idl_type *resolved_t2 = resolve_type(env, t2);

    if (!resolved_t1 || !resolved_t2) {
      idl_gamma_remove(gamma, t1, t2);
      return IDL_SUBTYPE_FAIL;
    }

    idl_subtype_result res =
        subtype_check_impl(report, gamma, env, resolved_t1, resolved_t2);

    if (res == IDL_SUBTYPE_FAIL) {
      idl_gamma_remove(gamma, t1, t2);
    }

    return res;
  }

  /* Resolve types for comparison */
  const idl_type *rt1 = resolve_type(env, t1);
  const idl_type *rt2 = resolve_type(env, t2);

  if (!rt1 || !rt2) {
    return IDL_SUBTYPE_FAIL;
  }

  /* Base rules */

  /* Everything is a subtype of reserved */
  if (rt2->kind == IDL_KIND_RESERVED) {
    return IDL_SUBTYPE_OK;
  }

  /* Empty is a subtype of everything */
  if (rt1->kind == IDL_KIND_EMPTY) {
    return IDL_SUBTYPE_OK;
  }

  /* nat <: int */
  if (rt1->kind == IDL_KIND_NAT && rt2->kind == IDL_KIND_INT) {
    return IDL_SUBTYPE_OK;
  }

  /* Same primitive types */
  if (rt1->kind == rt2->kind && idl_type_is_primitive(rt1)) {
    return IDL_SUBTYPE_OK;
  }

  /* vec T1 <: vec T2 if T1 <: T2 */
  if (rt1->kind == IDL_KIND_VEC && rt2->kind == IDL_KIND_VEC) {
    return subtype_check_impl(report, gamma, env, rt1->data.inner,
                              rt2->data.inner);
  }

  /* opt rules */
  if (rt2->kind == IDL_KIND_OPT) {
    /* null <: opt T */
    if (rt1->kind == IDL_KIND_NULL) {
      return IDL_SUBTYPE_OK;
    }

    /* opt T1 <: opt T2 if T1 <: T2 */
    if (rt1->kind == IDL_KIND_OPT) {
      idl_subtype_result inner_res = subtype_check_impl(
          report, gamma, env, rt1->data.inner, rt2->data.inner);
      if (inner_res == IDL_SUBTYPE_OK) {
        return IDL_SUBTYPE_OK;
      }
    }

    /* T <: opt T2 if T <: T2 and T2 is not null/reserved/opt */
    const idl_type *inner_t2 = resolve_type(env, rt2->data.inner);
    if (inner_t2 && !idl_type_is_optional_like(env, inner_t2)) {
      idl_subtype_result inner_res =
          subtype_check_impl(report, gamma, env, rt1, rt2->data.inner);
      if (inner_res == IDL_SUBTYPE_OK) {
        return IDL_SUBTYPE_OK;
      }
    }

    /* Special opt rule: anything can be coerced to opt (with warning) */
    switch (report) {
    case IDL_OPT_SILENCE:
      return IDL_SUBTYPE_OPT_SPECIAL;
    case IDL_OPT_WARNING:
      fprintf(stderr, "WARNING: subtype coercion via special opt rule\n");
      return IDL_SUBTYPE_OPT_SPECIAL;
    case IDL_OPT_ERROR:
      return IDL_SUBTYPE_FAIL;
    }
  }

  /* record {f1:T1; ...} <: record {f2:T2; ...} */
  if (rt1->kind == IDL_KIND_RECORD && rt2->kind == IDL_KIND_RECORD) {
    /* For each field in t2, check if t1 has a matching field */
    for (size_t i = 0; i < rt2->data.record.fields_len; i++) {
      idl_field *f2 = &rt2->data.record.fields[i];
      int found = 0;

      for (size_t j = 0; j < rt1->data.record.fields_len; j++) {
        idl_field *f1 = &rt1->data.record.fields[j];
        if (f1->label.id == f2->label.id) {
          /* Found matching field, check subtype */
          idl_subtype_result field_res =
              subtype_check_impl(report, gamma, env, f1->type, f2->type);
          if (field_res == IDL_SUBTYPE_FAIL) {
            return IDL_SUBTYPE_FAIL;
          }
          found = 1;
          break;
        }
      }

      if (!found) {
        /* Field not in t1; t2's field must be optional-like */
        if (!idl_type_is_optional_like(env, f2->type)) {
          return IDL_SUBTYPE_FAIL;
        }
      }
    }

    return IDL_SUBTYPE_OK;
  }

  /* variant {f1:T1; ...} <: variant {f2:T2; ...} */
  if (rt1->kind == IDL_KIND_VARIANT && rt2->kind == IDL_KIND_VARIANT) {
    /* For each field in t1, check if t2 has a matching field */
    for (size_t i = 0; i < rt1->data.record.fields_len; i++) {
      idl_field *f1 = &rt1->data.record.fields[i];
      int found = 0;

      for (size_t j = 0; j < rt2->data.record.fields_len; j++) {
        idl_field *f2 = &rt2->data.record.fields[j];
        if (f1->label.id == f2->label.id) {
          /* Found matching field, check subtype */
          idl_subtype_result field_res =
              subtype_check_impl(report, gamma, env, f1->type, f2->type);
          if (field_res == IDL_SUBTYPE_FAIL) {
            return IDL_SUBTYPE_FAIL;
          }
          found = 1;
          break;
        }
      }

      if (!found) {
        /* Field in t1 not in t2: not a subtype */
        return IDL_SUBTYPE_FAIL;
      }
    }

    return IDL_SUBTYPE_OK;
  }

  /* func and service subtyping - contravariant args, covariant returns */
  if (rt1->kind == IDL_KIND_FUNC && rt2->kind == IDL_KIND_FUNC) {
    idl_func *f1 = &((idl_type *)rt1)->data.func;
    idl_func *f2 = &((idl_type *)rt2)->data.func;

    /* Check arg count */
    if (f1->args_len != f2->args_len) {
      return IDL_SUBTYPE_FAIL;
    }

    /* Check return count */
    if (f1->rets_len != f2->rets_len) {
      return IDL_SUBTYPE_FAIL;
    }

    /* Args are contravariant: t2.args <: t1.args */
    for (size_t i = 0; i < f1->args_len; i++) {
      idl_subtype_result res =
          subtype_check_impl(report, gamma, env, f2->args[i], f1->args[i]);
      if (res == IDL_SUBTYPE_FAIL) {
        return IDL_SUBTYPE_FAIL;
      }
    }

    /* Returns are covariant: t1.rets <: t2.rets */
    for (size_t i = 0; i < f1->rets_len; i++) {
      idl_subtype_result res =
          subtype_check_impl(report, gamma, env, f1->rets[i], f2->rets[i]);
      if (res == IDL_SUBTYPE_FAIL) {
        return IDL_SUBTYPE_FAIL;
      }
    }

    /* Check modes match */
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

  /* service subtyping - methods must be subtypes */
  if (rt1->kind == IDL_KIND_SERVICE && rt2->kind == IDL_KIND_SERVICE) {
    idl_service *s1 = &((idl_type *)rt1)->data.service;
    idl_service *s2 = &((idl_type *)rt2)->data.service;

    /* For each method in s2, s1 must have a matching method */
    for (size_t i = 0; i < s2->methods_len; i++) {
      idl_method *m2 = &s2->methods[i];
      int found = 0;

      for (size_t j = 0; j < s1->methods_len; j++) {
        idl_method *m1 = &s1->methods[j];
        if (strcmp(m1->name, m2->name) == 0) {
          /* Found matching method, check subtype */
          idl_subtype_result res =
              subtype_check_impl(report, gamma, env, m1->type, m2->type);
          if (res == IDL_SUBTYPE_FAIL) {
            return IDL_SUBTYPE_FAIL;
          }
          found = 1;
          break;
        }
      }

      if (!found) {
        return IDL_SUBTYPE_FAIL;
      }
    }

    return IDL_SUBTYPE_OK;
  }

  /* No match */
  return IDL_SUBTYPE_FAIL;
}

idl_subtype_result idl_subtype_check(idl_opt_report report, idl_gamma *gamma,
                                     const idl_type_env *env,
                                     const idl_type *t1, const idl_type *t2) {
  if (!gamma || !t1 || !t2) {
    return IDL_SUBTYPE_FAIL;
  }

  return subtype_check_impl(report, gamma, env, t1, t2);
}

idl_subtype_result idl_subtype(const idl_type_env *env, const idl_type *t1,
                               const idl_type *t2, idl_arena *arena) {
  if (!t1 || !t2 || !arena) {
    return IDL_SUBTYPE_FAIL;
  }

  idl_gamma gamma;
  if (idl_gamma_init(&gamma, arena) != IDL_STATUS_OK) {
    return IDL_SUBTYPE_FAIL;
  }

  return idl_subtype_check(IDL_OPT_WARNING, &gamma, env, t1, t2);
}
