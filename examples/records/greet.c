#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

#include "ic_candid_builder.h"
#include "idl/candid.h"
#include <tinyprintf.h>

// -----------------------------------------------------------------------------
// Query function: greet (text) -> (text)
// Uses simplified argument parsing API (IC_API_PARSE_SIMPLE)
// -----------------------------------------------------------------------------
IC_API_QUERY(greet, "(text) -> (text)") {
    // Echo-friendly greeting using simplified argument parsing
    char *name = NULL;
    IC_API_PARSE_SIMPLE(api, text, name);

    char reply[256];
    tfp_snprintf(reply, sizeof(reply), "Hello, %s! Welcome to IC C SDK.", name);
    IC_API_REPLY_TEXT(reply);
}

// -----------------------------------------------------------------------------
// Update: add_user (text, nat, bool) -> (text)
// Uses simplified argument parsing API
// (IC_API_PARSE_BEGIN/IC_ARG_*/IC_API_PARSE_END)
// -----------------------------------------------------------------------------
IC_API_UPDATE(add_user, "(text, nat, bool) -> (text)") {
    char    *name = NULL;
    uint64_t age = 0;
    bool     active = false;

    IC_API_PARSE_BEGIN(api);
    IC_ARG_TEXT(&name);
    IC_ARG_NAT(&age);
    IC_ARG_BOOL(&active);
    IC_API_PARSE_END();

    // All arguments parsed and ready to use
    // Memory for 'name' is automatically managed
    char reply[256];
    tfp_snprintf(reply, sizeof(reply),
                 "User '%s' (age: %llu, active: %s) added successfully.", name,
                 (unsigned long long)age, active ? "yes" : "no");
    IC_API_REPLY_TEXT(reply);
}

// -----------------------------------------------------------------------------
// Query: get_address (text) -> (opt record { street : text; city : text; zip :
// nat }) Demonstrates Solution 1: Macro-based API with simplified syntax
// -----------------------------------------------------------------------------
IC_API_QUERY(
    get_address,
    "(text) -> (opt record { street : text; city : text; zip : nat })") {
    char *name = NULL;
    IC_API_PARSE_SIMPLE(api, text, name);

    IC_API_BUILDER_BEGIN(api) {
        // Build type using simplified macro API (fields auto-sorted)
        idl_type *address_type =
            IDL_RECORD(arena, IDL_FIELD("street", idl_type_text(arena)),
                       IDL_FIELD("city", idl_type_text(arena)),
                       IDL_FIELD("zip", idl_type_nat(arena)));
        idl_type *opt_addr = idl_type_opt(arena, address_type);

        // Build value using simplified macro API (fields auto-sorted)
        char street[128], city[64];
        tfp_snprintf(street, sizeof(street), "%s Street", name);
        tfp_snprintf(city, sizeof(city), "%s City", name);
        uint32_t zip = 10000 + (uint32_t)(strlen(name) % 90000);

        idl_value *addr_val = IDL_RECORD_VALUE(
            arena,
            IDL_VALUE_FIELD("street", idl_value_text_cstr(arena, street)),
            IDL_VALUE_FIELD("city", idl_value_text_cstr(arena, city)),
            IDL_VALUE_FIELD("zip", idl_value_nat32(arena, zip)));
        idl_value *opt_val = idl_value_opt_some(arena, addr_val);

        idl_builder_arg(builder, opt_addr, opt_val);
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Query: get_profile (text) -> (record { name : text; age : nat; active : bool
// }) Demonstrates Solution 2: Builder API with type-safe helpers
// -----------------------------------------------------------------------------
IC_API_QUERY(get_profile,
             "(text) -> (record { name : text; age : nat; active : bool })") {
    char *username = NULL;
    IC_API_PARSE_SIMPLE(api, text, username);

    IC_API_BUILDER_BEGIN(api) {
        // Use builder API for type and value construction (auto-sorted)
        idl_record_builder *rb = idl_record_builder_new(arena, 3);
        idl_record_builder_text(rb, "name", username);
        idl_record_builder_nat32(rb, "age", 25);
        idl_record_builder_bool(rb, "active", true);

        idl_type  *type = idl_record_builder_build_type(rb);
        idl_value *val = idl_record_builder_build_value(rb);

        idl_builder_arg(builder, type, val);
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Query: get_user_info (text) -> (record { id : nat; emails : vec text; tags :
// vec text }) Demonstrates vector construction with simplified API
// -----------------------------------------------------------------------------
IC_API_QUERY(
    get_user_info,
    "(text) -> (record { id : nat; emails : vec text; tags : vec text })") {
    char *username = NULL;
    IC_API_PARSE_SIMPLE(api, text, username);

    IC_API_BUILDER_BEGIN(api) {
        // Build vectors using IDL_VEC macro
        idl_value *emails = IDL_VEC(arena, V_TEXT("primary@example.com"),
                                    V_TEXT("secondary@example.com"));

        idl_value *tags = IDL_VEC(arena, V_TEXT("developer"), V_TEXT("admin"));

        // Build record with vectors (auto-sorted)
        idl_type *info_type = IDL_RECORD(arena, IDL_FIELD("id", T_NAT),
                                         IDL_FIELD("emails", T_VEC(T_TEXT)),
                                         IDL_FIELD("tags", T_VEC(T_TEXT)));

        idl_value *info_val = IDL_RECORD_VALUE(
            arena, IDL_VALUE_FIELD("id", V_NAT32(42)),
            IDL_VALUE_FIELD("emails", emails), IDL_VALUE_FIELD("tags", tags));

        idl_builder_arg(builder, info_type, info_val);
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Query: get_nested_data (text) -> (record { user : record { name : text; age :
// nat }; timestamp : nat }) Demonstrates nested record construction
// -----------------------------------------------------------------------------
IC_API_QUERY(get_nested_data,
             "(text) -> (record { user : record { name : text; age : nat }; "
             "timestamp : nat })") {
    char *username = NULL;
    IC_API_PARSE_SIMPLE(api, text, username);

    IC_API_BUILDER_BEGIN(api) {
        // Build nested user record (auto-sorted)
        idl_type *user_type = IDL_RECORD(arena, IDL_FIELD("name", T_TEXT),
                                         IDL_FIELD("age", T_NAT));

        idl_value *user_val =
            IDL_RECORD_VALUE(arena, IDL_VALUE_FIELD("name", V_TEXT(username)),
                             IDL_VALUE_FIELD("age", V_NAT32(30)));

        // Build outer record with nested record (auto-sorted)
        idl_type *data_type = IDL_RECORD(arena, IDL_FIELD("user", user_type),
                                         IDL_FIELD("timestamp", T_NAT));

        idl_value *data_val =
            IDL_RECORD_VALUE(arena, IDL_VALUE_FIELD("user", user_val),
                             IDL_VALUE_FIELD("timestamp", V_NAT64(1704384000)));

        idl_builder_arg(builder, data_type, data_val);
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Query: get_optional_data (text, bool) -> (record { name : text; age : opt nat
// }) Demonstrates optional field handling
// -----------------------------------------------------------------------------
IC_API_QUERY(get_optional_data,
             "(text, bool) -> (record { name : text; age : opt nat })") {
    char *username = NULL;
    bool  include_age = false;

    IC_API_PARSE_BEGIN(api);
    IC_ARG_TEXT(&username);
    IC_ARG_BOOL(&include_age);
    IC_API_PARSE_END();

    IC_API_BUILDER_BEGIN(api) {
        // Build record with optional field (auto-sorted)
        idl_type *data_type = IDL_RECORD(arena, IDL_FIELD("name", T_TEXT),
                                         IDL_FIELD("age", T_OPT(T_NAT)));

        idl_value *age_val = include_age ? V_SOME(V_NAT32(25)) : V_NONE;

        idl_value *data_val =
            IDL_RECORD_VALUE(arena, IDL_VALUE_FIELD("name", V_TEXT(username)),
                             IDL_VALUE_FIELD("age", age_val));

        idl_builder_arg(builder, data_type, data_val);
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Query: get_complex_record (text) -> (record with all types)
// Demonstrates builder API with all supported types
// -----------------------------------------------------------------------------
IC_API_QUERY(
    get_complex_record,
    "(text) -> (record { name : text; active : bool; score : nat32; balance : "
    "nat64; temp : int32; offset : int64; ratio : float32; pi : float64 })") {
    char *username = NULL;
    IC_API_PARSE_SIMPLE(api, text, username);

    IC_API_BUILDER_BEGIN(api) {
        // Use builder API to demonstrate all type helpers (auto-sorted)
        idl_record_builder *rb = idl_record_builder_new(arena, 8);
        idl_record_builder_text(rb, "name", username);
        idl_record_builder_bool(rb, "active", true);
        idl_record_builder_nat32(rb, "score", 100);
        idl_record_builder_nat64(rb, "balance", 1000000);
        idl_record_builder_int32(rb, "temp", -10);
        idl_record_builder_int64(rb, "offset", -1000000);
        idl_record_builder_float32(rb, "ratio", 0.75f);
        idl_record_builder_float64(rb, "pi", 3.14159);

        idl_type  *type = idl_record_builder_build_type(rb);
        idl_value *val = idl_record_builder_build_value(rb);

        idl_builder_arg(builder, type, val);
    }
    IC_API_BUILDER_END(api);
}
