#ifndef IDL_TYPE_ENV_H
#define IDL_TYPE_ENV_H

#include "arena.h"
#include "base.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Type environment entry: maps a name to a type.
 */
typedef struct idl_type_env_entry {
    const char                *name;
    idl_type                  *type;
    struct idl_type_env_entry *next; /* for hash collision chaining */
} idl_type_env_entry;

/*
 * Type environment: stores named type bindings.
 * Uses a simple hash table with chaining.
 */
typedef struct idl_type_env {
    idl_arena           *arena;
    idl_type_env_entry **buckets;
    size_t               bucket_count;
    size_t               count;
} idl_type_env;

/*
 * Initialize a type environment.
 */
idl_status idl_type_env_init(idl_type_env *env, idl_arena *arena);

/*
 * Insert a type binding. Returns error if name already exists with different
 * type.
 */
idl_status
idl_type_env_insert(idl_type_env *env, const char *name, idl_type *type);

/*
 * Find a type by name. Returns NULL if not found.
 */
idl_type *idl_type_env_find(const idl_type_env *env, const char *name);

/*
 * Recursively find a type, following VAR references.
 */
idl_type *idl_type_env_rec_find(const idl_type_env *env, const char *name);

/*
 * Trace a type through VAR references to get the underlying type.
 */
idl_type *idl_type_env_trace(const idl_type_env *env, const idl_type *type);

/*
 * Get the number of entries in the environment.
 */
size_t idl_type_env_count(const idl_type_env *env);

#ifdef __cplusplus
}
#endif

#endif /* IDL_TYPE_ENV_H */
