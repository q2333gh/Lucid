#ifndef IDL_SUBTYPE_H
#define IDL_SUBTYPE_H

#include "arena.h"
#include "base.h"
#include "type_env.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Subtype checking result.
 */
typedef enum idl_subtype_result {
  IDL_SUBTYPE_OK = 0,
  IDL_SUBTYPE_FAIL,
  IDL_SUBTYPE_OPT_SPECIAL, /* special opt rule applied */
} idl_subtype_result;

/*
 * Error reporting style for the special opt rule.
 */
typedef enum idl_opt_report {
  IDL_OPT_SILENCE,
  IDL_OPT_WARNING,
  IDL_OPT_ERROR,
} idl_opt_report;

/*
 * Gamma cache entry for recursive subtype checking.
 * Uses a simple hash set with chaining.
 */
typedef struct idl_gamma_entry {
  const idl_type *t1;
  const idl_type *t2;
  struct idl_gamma_entry *next;
} idl_gamma_entry;

typedef struct idl_gamma {
  idl_arena *arena;
  idl_gamma_entry **buckets;
  size_t bucket_count;
  size_t count;
} idl_gamma;

/*
 * Initialize a Gamma cache.
 */
idl_status idl_gamma_init(idl_gamma *gamma, idl_arena *arena);

/*
 * Check if a pair is in the Gamma cache.
 */
int idl_gamma_contains(const idl_gamma *gamma, const idl_type *t1,
                       const idl_type *t2);

/*
 * Insert a pair into the Gamma cache. Returns 1 if newly inserted, 0 if
 * already present.
 */
int idl_gamma_insert(idl_gamma *gamma, const idl_type *t1, const idl_type *t2);

/*
 * Remove a pair from the Gamma cache.
 */
void idl_gamma_remove(idl_gamma *gamma, const idl_type *t1, const idl_type *t2);

/*
 * Check if t1 <: t2 (t1 is a subtype of t2).
 *
 * @param report  How to handle the special opt rule.
 * @param gamma   Memoization cache for recursive types.
 * @param env     Type environment for resolving VAR types.
 * @param t1      The wire type (what we have).
 * @param t2      The expected type (what we want).
 */
idl_subtype_result idl_subtype_check(idl_opt_report report, idl_gamma *gamma,
                                     const idl_type_env *env,
                                     const idl_type *t1, const idl_type *t2);

/*
 * Simplified subtype check (uses default settings).
 */
idl_subtype_result idl_subtype(const idl_type_env *env, const idl_type *t1,
                               const idl_type *t2, idl_arena *arena);

/*
 * Check if a type is "optional-like" (opt, null, or reserved).
 */
int idl_type_is_optional_like(const idl_type_env *env, const idl_type *type);

#ifdef __cplusplus
}
#endif

#endif /* IDL_SUBTYPE_H */
