// Main header file for IC C SDK
// Include this file to get all IC SDK functionality

#pragma once

// Core types
#include "ic_entry_points.h"
#include "ic_types.h"

// IC0 system calls
#include "ic0.h"

// Buffer management
#include "ic_buffer.h"

// Principal handling
#include "ic_principal.h"

// Candid serialization
#include "ic_candid.h"
#include "ic_candid_did.h"
#include "ic_candid_registry.h"

// High-level API
#include "ic_api.h"
#include "ic_simple.h"

// Convenience macros for exporting canister entry points (for function
// definitions) Usage: IC_EXPORT_QUERY(greet_no_arg) { ... } expands to:
//   __attribute__((export_name("canister_query greet_no_arg")))
//   __attribute__((visibility("default"))) void greet_no_arg(void) { ... }
#define IC_EXPORT_QUERY(func)                                                  \
    __attribute__((export_name("canister_query " #func)))                      \
    __attribute__((visibility("default"))) void                                \
    func(void)
#define IC_EXPORT_UPDATE(func)                                                 \
    __attribute__((export_name("canister_update " #func)))                     \
    __attribute__((visibility("default"))) void                                \
    func(void)
