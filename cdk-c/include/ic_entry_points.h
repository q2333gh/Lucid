// Entry point type definitions
#pragma once

#include "ic_types.h"

// Entry point types matching IC specification
typedef enum {
    IC_ENTRY_INIT,             // I: canister_init
    IC_ENTRY_POST_UPGRADE,     // I: canister_post_upgrade
    IC_ENTRY_PRE_UPGRADE,      // G: canister_pre_upgrade
    IC_ENTRY_UPDATE,           // U: canister_update
    IC_ENTRY_QUERY,            // Q: canister_query
    IC_ENTRY_REPLY_CALLBACK,   // Ry: reply callback
    IC_ENTRY_REJECT_CALLBACK,  // Rt: reject callback
    IC_ENTRY_CLEANUP_CALLBACK, // C: cleanup callback
    IC_ENTRY_HEARTBEAT,        // T: canister_heartbeat
    IC_ENTRY_GLOBAL_TIMER,     // T: canister_global_timer
    IC_ENTRY_INSPECT_MESSAGE,  // F: canister_inspect_message
    IC_ENTRY_START             // s: start function
} ic_entry_type_t;

// Helper functions to check entry point types
static inline bool ic_entry_is_I(ic_entry_type_t type) {
    return type == IC_ENTRY_INIT || type == IC_ENTRY_POST_UPGRADE;
}

static inline bool ic_entry_is_G(ic_entry_type_t type) { return type == IC_ENTRY_PRE_UPGRADE; }

static inline bool ic_entry_is_U(ic_entry_type_t type) { return type == IC_ENTRY_UPDATE; }

static inline bool ic_entry_is_Q(ic_entry_type_t type) { return type == IC_ENTRY_QUERY; }

static inline bool ic_entry_is_Ry(ic_entry_type_t type) { return type == IC_ENTRY_REPLY_CALLBACK; }

static inline bool ic_entry_is_Rt(ic_entry_type_t type) { return type == IC_ENTRY_REJECT_CALLBACK; }

static inline bool ic_entry_can_reply(ic_entry_type_t type) {
    return ic_entry_is_U(type) || ic_entry_is_Q(type) || ic_entry_is_Ry(type) ||
           ic_entry_is_Rt(type);
}
