/*
 * Type Environment (Symbol Table)
 *
 * Implements a hash table for mapping type names to type definitions.
 * Used for:
 * - Type variable resolution (VAR types reference named types)
 * - Type table lookup during deserialization
 * - Type environment management in builders and deserializers
 *
 * The type environment allows Candid to support named types and type
 * references, enabling recursive types and type reuse in complex schemas.
 */
#include "idl/type_env.h"

#include <string.h>

#define INITIAL_BUCKET_COUNT 32

/* Simple string hash for bucket selection */
static size_t hash_string(const char *s) {
    size_t h = 5381;
    while (*s) {
        h = ((h << 5) + h) + (unsigned char)*s;
        s++;
    }
    return h;
}

idl_status idl_type_env_init(idl_type_env *env, idl_arena *arena) {
    if (!env || !arena) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    env->arena = arena;
    env->bucket_count = INITIAL_BUCKET_COUNT;
    env->count = 0;
    env->buckets = idl_arena_alloc_zeroed(arena, sizeof(idl_type_env_entry *) *
                                                     env->bucket_count);

    if (!env->buckets) {
        return IDL_STATUS_ERR_ALLOC;
    }

    return IDL_STATUS_OK;
}

idl_status
idl_type_env_insert(idl_type_env *env, const char *name, idl_type *type) {
    if (!env || !name || !type) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }

    size_t bucket = hash_string(name) % env->bucket_count;

    /* Check if name already exists */
    for (idl_type_env_entry *e = env->buckets[bucket]; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            /* Name exists; check if types match */
            if (e->type != type) {
                return IDL_STATUS_ERR_INVALID_ARG; /* inconsistent binding */
            }
            return IDL_STATUS_OK; /* already inserted */
        }
    }

    /* Create new entry */
    idl_type_env_entry *entry =
        idl_arena_alloc(env->arena, sizeof(idl_type_env_entry));
    if (!entry) {
        return IDL_STATUS_ERR_ALLOC;
    }

    /* Duplicate name into arena */
    size_t name_len = strlen(name) + 1;
    char  *name_copy = idl_arena_alloc(env->arena, name_len);
    if (!name_copy) {
        return IDL_STATUS_ERR_ALLOC;
    }
    memcpy(name_copy, name, name_len);

    entry->name = name_copy;
    entry->type = type;
    entry->next = env->buckets[bucket];
    env->buckets[bucket] = entry;
    env->count++;

    return IDL_STATUS_OK;
}

idl_type *idl_type_env_find(const idl_type_env *env, const char *name) {
    if (!env || !name) {
        return NULL;
    }

    size_t bucket = hash_string(name) % env->bucket_count;

    for (idl_type_env_entry *e = env->buckets[bucket]; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            return e->type;
        }
    }

    return NULL;
}

idl_type *idl_type_env_rec_find(const idl_type_env *env, const char *name) {
    idl_type *t = idl_type_env_find(env, name);
    if (!t) {
        return NULL;
    }

    /* Follow VAR references */
    while (t && t->kind == IDL_KIND_VAR) {
        t = idl_type_env_find(env, t->data.var_name);
    }

    return t;
}

idl_type *idl_type_env_trace(const idl_type_env *env, const idl_type *type) {
    if (!env || !type) {
        return NULL;
    }

    /* Follow VAR references */
    while (type && type->kind == IDL_KIND_VAR) {
        type = idl_type_env_find(env, type->data.var_name);
    }

    return (idl_type *)type;
}

size_t idl_type_env_count(const idl_type_env *env) {
    return env ? env->count : 0;
}
