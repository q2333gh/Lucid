#ifndef IDL_RUNTIME_H
#define IDL_RUNTIME_H

/* Phase 1: Core utilities */
#include "arena.h"
#include "base.h"
#include "hash.h"
#include "leb128.h"

/* Phase 2: Type system and header */
#include "header.h"
#include "type_env.h"
#include "type_table.h"
#include "types.h"

/* Phase 3: Value serialization */
#include "builder.h"
#include "value.h"
#include "value_serializer.h"

/* Phase 4: Value deserialization */
#include "deserializer.h"

/* Phase 5: Subtype and coercion */
#include "coerce.h"
#include "subtype.h"

#endif /* IDL_RUNTIME_H */
