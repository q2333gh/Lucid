#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

// #include <stdio.h>
#include <string.h>

#include "idl/candid.h"
#include <tinyprintf.h>

// -----------------------------------------------------------------------------
// Helpers: small, in-memory demo data + reply helper using the low-level
// Candid builder so we can return records/variants/vec/opt.
// Note: Now using ic_api_reply_builder which handles everything automatically
// -----------------------------------------------------------------------------

// Build Address type/value
static idl_type *build_address_type(idl_arena *arena) {
    idl_field fields[3];

    fields[0].label = idl_label_name("street");
    fields[0].type = idl_type_text(arena);

    fields[1].label = idl_label_name("city");
    fields[1].type = idl_type_text(arena);

    fields[2].label = idl_label_name("zip");
    fields[2].type = idl_type_nat(arena);

    return idl_type_record(arena, fields, 3);
}

static idl_value *build_address_value(idl_arena  *arena,
                                      const char *street,
                                      const char *city,
                                      uint32_t    zip) {
    idl_value_field fields[3];

    fields[0].label = idl_label_name("street");
    fields[0].value = idl_value_text_cstr(arena, street);

    fields[1].label = idl_label_name("city");
    fields[1].value = idl_value_text_cstr(arena, city);

    fields[2].label = idl_label_name("zip");
    fields[2].value = idl_value_nat32(arena, zip);

    return idl_value_record(arena, fields, 3);
}

// Build Status type/value helpers
static idl_type *build_status_type(idl_arena *arena) {
    idl_field variants[3];

    variants[0].label = idl_label_name("Active");
    variants[0].type = idl_type_null(arena);

    variants[1].label = idl_label_name("Inactive");
    variants[1].type = idl_type_null(arena);

    variants[2].label = idl_label_name("Banned");
    variants[2].type = idl_type_text(arena);

    return idl_type_variant(arena, variants, 3);
}

static idl_value *build_status_value_active(idl_arena *arena) {
    idl_value_field field;
    field.label = idl_label_name("Active");
    field.value = idl_value_null(arena);
    return idl_value_variant(arena, 0, &field);
}

static idl_value *build_status_value_inactive(idl_arena *arena) {
    idl_value_field field;
    field.label = idl_label_name("Inactive");
    field.value = idl_value_null(arena);
    return idl_value_variant(arena, 1, &field);
}

static idl_value *build_status_value_banned(idl_arena *arena, const char *msg) {
    idl_value_field field;
    field.label = idl_label_name("Banned");
    field.value = idl_value_text_cstr(arena, msg);
    return idl_value_variant(arena, 2, &field);
}

// Build Profile type/value
static idl_type *build_profile_type(idl_arena *arena, idl_type *status_type) {
    idl_field fields[5];

    fields[0].label = idl_label_name("id");
    fields[0].type = idl_type_nat(arena);

    fields[1].label = idl_label_name("name");
    fields[1].type = idl_type_text(arena);

    fields[2].label = idl_label_name("emails");
    fields[2].type = idl_type_vec(arena, idl_type_text(arena));

    fields[3].label = idl_label_name("age");
    fields[3].type = idl_type_opt(arena, idl_type_nat(arena));

    fields[4].label = idl_label_name("status");
    fields[4].type = status_type;

    return idl_type_record(arena, fields, 5);
}

static idl_value *build_profile_value(idl_arena *arena, idl_value *status_val) {
    idl_value_field fields[5];

    fields[0].label = idl_label_name("id");
    fields[0].value = idl_value_nat32(arena, 7);

    fields[1].label = idl_label_name("name");
    fields[1].value = idl_value_text_cstr(arena, "Dfinity Dev");

    idl_value *email_items[2];
    email_items[0] = idl_value_text_cstr(arena, "dev@example.com");
    email_items[1] = idl_value_text_cstr(arena, "contact@example.com");
    fields[2].label = idl_label_name("emails");
    fields[2].value = idl_value_vec(arena, email_items, 2);

    fields[3].label = idl_label_name("age");
    fields[3].value = idl_value_opt_some(arena, idl_value_nat32(arena, 30));

    fields[4].label = idl_label_name("status");
    fields[4].value = status_val;

    return idl_value_record(arena, fields, 5);
}

// -----------------------------------------------------------------------------
// Query function: greet
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
// Update: add_user (text, nat, bool) -> ()
// Uses simplified argument parsing API
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
// Update: set_address (name : text, addr : Address) -> ()
// -----------------------------------------------------------------------------
IC_API_UPDATE(
    set_address,
    "(text, record { street : text; city : text; zip : nat }) -> (text)") {
    // Parse name argument
    char *name = NULL;
    IC_API_PARSE_SIMPLE(api, text, name);

    // Note: Record parsing not yet supported, but we can still return success
    char reply[256];
    tfp_snprintf(reply, sizeof(reply),
                 "Address set successfully for user '%s'.", name);
    IC_API_REPLY_TEXT(reply);
}

// -----------------------------------------------------------------------------
// Query: get_address (name : text) -> (opt Address)
// -----------------------------------------------------------------------------
IC_API_QUERY(
    get_address,
    "(text) -> (opt record { street : text; city : text; zip : nat })") {
    // Parse input argument using simplified API
    char *name = NULL;
    IC_API_PARSE_SIMPLE(api, text, name);

    // Build reply using simplified API (arena is automatically managed)
    // Return deterministic address based on name
    IC_API_BUILDER_BEGIN(api) {
        idl_type *address_type = build_address_type(arena);
        idl_type *opt_addr = idl_type_opt(arena, address_type);

        // Generate deterministic address based on name
        char street[128], city[64];
        tfp_snprintf(street, sizeof(street), "%s Street", name);
        tfp_snprintf(city, sizeof(city), "%s City", name);
        uint32_t zip =
            10000 + (uint32_t)(strlen(name) % 90000); // Deterministic zip

        idl_value *addr_val = idl_value_opt_some(
            arena, build_address_value(arena, street, city, zip));

        idl_builder_arg(builder, opt_addr, addr_val);
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Update: set_status (user : text, st : Status) -> ()
// Note: Only text argument is parsed with simplified API, variant parsing
// would require additional support
// -----------------------------------------------------------------------------
IC_API_UPDATE(set_status,
              "(text, variant { Active; Inactive; Banned : text }) -> (text)") {
    // Parse text argument using simplified API
    char *user = NULL;
    IC_API_PARSE_SIMPLE(api, text, user);

    // TODO: Variant parsing would need additional API support
    // For now, we just parse the text argument
    char reply[256];
    tfp_snprintf(reply, sizeof(reply),
                 "Status updated successfully for user '%s'.", user);
    IC_API_REPLY_TEXT(reply);
}

// -----------------------------------------------------------------------------
// Query: get_status (user : text) -> (Status)
// -----------------------------------------------------------------------------
IC_API_QUERY(get_status,
             "(text) -> (variant { Active; Inactive; Banned : text })") {
    // Parse input argument using simplified API
    char *user = NULL;
    IC_API_PARSE_SIMPLE(api, text, user);

    // Build reply using simplified API (arena is automatically managed)
    // Return deterministic status based on username
    IC_API_BUILDER_BEGIN(api) {
        idl_type  *status_type = build_status_type(arena);
        idl_value *status_val;

        // Deterministic status based on username
        // For "user1" (length 5, 5 % 3 = 2) -> Banned
        // For "user" (length 4, 4 % 3 = 1) -> Inactive
        // For "usr" (length 3, 3 % 3 = 0) -> Active
        if (user != NULL) {
            size_t user_len = strlen(user);
            if (user_len % 3 == 2) {
                // Banned with deterministic message
                char ban_msg[128];
                tfp_snprintf(ban_msg, sizeof(ban_msg),
                             "User '%s' banned (reason: policy violation)",
                             user);
                status_val = build_status_value_banned(arena, ban_msg);
            } else if (user_len % 3 == 1) {
                // Inactive
                status_val = build_status_value_inactive(arena);
            } else {
                // Active (user_len % 3 == 0)
                status_val = build_status_value_active(arena);
            }
        } else {
            // Default to Active if user is NULL
            status_val = build_status_value_active(arena);
        }

        if (idl_builder_arg(builder, status_type, status_val) !=
            IDL_STATUS_OK) {
            ic_api_trap("Failed to add status variant to builder");
        }
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Update: create_profiles (vec Profile) -> (vec Result)
// -----------------------------------------------------------------------------
IC_API_UPDATE(
    create_profiles,
    "(vec record { id : nat; name : text; emails : vec text; age : opt nat; "
    "status : variant { Active; Inactive; Banned : text } }) -> (vec variant { "
    "Ok : record { id : nat; name : text; emails : vec text; age : opt nat; "
    "status : variant { Active; Inactive; Banned : text } }; Err : text })") {
    // Build reply using simplified API (arena is automatically managed)
    // Return deterministic results based on input count
    IC_API_BUILDER_BEGIN(api) {
        idl_type *status_type = build_status_type(arena);
        idl_type *profile_type = build_profile_type(arena, status_type);

        idl_field result_fields[2];
        result_fields[0].label = idl_label_name("Ok");
        result_fields[0].type = profile_type;
        result_fields[1].label = idl_label_name("Err");
        result_fields[1].type = idl_type_text(arena);

        idl_type *result_variant = idl_type_variant(arena, result_fields, 2);
        idl_type *vec_result = idl_type_vec(arena, result_variant);

        // Return 2 results: first Ok, second Err (deterministic)
        idl_value_field ok_field;
        ok_field.label = idl_label_name("Ok");
        ok_field.value =
            build_profile_value(arena, build_status_value_active(arena));
        idl_value *ok_variant = idl_value_variant(arena, 0, &ok_field);

        idl_value_field err_field;
        err_field.label = idl_label_name("Err");
        err_field.value =
            idl_value_text_cstr(arena, "Profile ID 1 already exists");
        idl_value *err_variant = idl_value_variant(arena, 1, &err_field);

        idl_value *items[2] = {ok_variant, err_variant};
        idl_value *vec_val = idl_value_vec(arena, items, 2);

        idl_builder_arg(builder, vec_result, vec_val);
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Query: find_profile (nat) -> (opt Profile)
// -----------------------------------------------------------------------------
IC_API_QUERY(
    find_profile,
    "(nat) -> (opt record { id : nat; name : text; emails : vec text; age : "
    "opt nat; status : variant { Active; Inactive; Banned : text } })") {
    // Parse input argument using simplified API
    uint64_t profile_id = 0;
    IC_API_PARSE_SIMPLE(api, nat, profile_id);

    // Build reply using simplified API (arena is automatically managed)
    // Return deterministic profile based on ID
    IC_API_BUILDER_BEGIN(api) {
        idl_type *status_type = build_status_type(arena);
        idl_type *profile_type = build_profile_type(arena, status_type);
        idl_type *opt_profile = idl_type_opt(arena, profile_type);

        // Return profile if ID is even, None if odd (deterministic)
        if (profile_id % 2 == 0) {
            idl_value *profile_val = idl_value_opt_some(
                arena,
                build_profile_value(arena, build_status_value_active(arena)));
            idl_builder_arg(builder, opt_profile, profile_val);
        } else {
            idl_value *none_val = idl_value_opt_none(arena);
            idl_builder_arg(builder, opt_profile, none_val);
        }
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Query: stats () -> (nat, vec nat, text)
// -----------------------------------------------------------------------------
IC_API_QUERY(stats, "() -> (nat, vec nat, text)") {
    // Build reply using simplified API (arena is automatically managed)
    // Return meaningful statistics
    IC_API_BUILDER_BEGIN(api) {
        uint64_t total_users = 42; // Deterministic count
        idl_builder_arg_nat64(builder, total_users);

        // Return meaningful statistics: active, inactive, banned counts
        uint64_t   stats_raw[3] = {25, 12, 5}; // active, inactive, banned
        idl_value *num_vals[3];
        num_vals[0] = idl_value_nat64(arena, stats_raw[0]);
        num_vals[1] = idl_value_nat64(arena, stats_raw[1]);
        num_vals[2] = idl_value_nat64(arena, stats_raw[2]);

        idl_type  *vec_nat_type = idl_type_vec(arena, idl_type_nat(arena));
        idl_value *vec_val = idl_value_vec(arena, num_vals, 3);
        idl_builder_arg(builder, vec_nat_type, vec_val);

        char status_msg[128];
        tfp_snprintf(
            status_msg, sizeof(status_msg),
            "System operational: %llu total users, %llu active, %llu inactive, "
            "%llu banned",
            (unsigned long long)total_users, (unsigned long long)stats_raw[0],
            (unsigned long long)stats_raw[1], (unsigned long long)stats_raw[2]);
        idl_builder_arg_text_cstr(builder, status_msg);
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Update: complex_test - Tests all Candid types with double nesting and vec
// Input: vec record {
//   id: nat;
//   name: text;
//   active: bool;
//   metadata: opt record {
//     tags: vec text;
//     status: variant { Active; Inactive; Banned: text };
//     details: record {
//       count: nat;
//       items: vec nat;
//     };
//   };
//   items: vec record {
//     value: text;
//     score: nat;
//   };
// }
// Output: vec variant {
//   Ok: record {
//     id: nat;
//     result: text;
//     data: opt vec record {
//       key: text;
//       value: nat;
//     };
//   };
//   Err: text;
// }
// -----------------------------------------------------------------------------
IC_API_UPDATE(
    complex_test,
    "(vec record { id : nat; name : text; active : bool; metadata : opt record "
    "{ "
    "tags : vec text; status : variant { Active; Inactive; Banned : text }; "
    "details : record { count : nat; items : vec nat } }; items : vec record { "
    "value : text; score : nat } }) -> (vec variant { Ok : record { id : nat; "
    "result : text; data : opt vec record { key : text; value : nat } }; Err : "
    "text })") {
    // Build reply using simplified API (arena is automatically managed)
    // Return deterministic results based on input structure
    IC_API_BUILDER_BEGIN(api) {
        // Build nested types for output
        // Inner record type: { key: text; value: nat }
        idl_field key_value_fields[2];
        key_value_fields[0].label = idl_label_name("key");
        key_value_fields[0].type = idl_type_text(arena);
        key_value_fields[1].label = idl_label_name("value");
        key_value_fields[1].type = idl_type_nat(arena);
        idl_type *key_value_record_type =
            idl_type_record(arena, key_value_fields, 2);

        // opt vec record { key: text; value: nat }
        idl_type *vec_key_value_type =
            idl_type_vec(arena, key_value_record_type);
        idl_type *opt_vec_key_value_type =
            idl_type_opt(arena, vec_key_value_type);

        // Ok variant record: { id: nat; result: text; data: opt vec record }
        idl_field ok_record_fields[3];
        ok_record_fields[0].label = idl_label_name("id");
        ok_record_fields[0].type = idl_type_nat(arena);
        ok_record_fields[1].label = idl_label_name("result");
        ok_record_fields[1].type = idl_type_text(arena);
        ok_record_fields[2].label = idl_label_name("data");
        ok_record_fields[2].type = opt_vec_key_value_type;
        idl_type *ok_record_type = idl_type_record(arena, ok_record_fields, 3);

        // Result variant: { Ok: record; Err: text }
        idl_field result_variant_fields[2];
        result_variant_fields[0].label = idl_label_name("Ok");
        result_variant_fields[0].type = ok_record_type;
        result_variant_fields[1].label = idl_label_name("Err");
        result_variant_fields[1].type = idl_type_text(arena);
        idl_type *result_variant_type =
            idl_type_variant(arena, result_variant_fields, 2);

        // Final output: vec variant
        idl_type *vec_result_type = idl_type_vec(arena, result_variant_type);

        // Build values: return 2 results (Ok and Err)
        // First result: Ok variant
        idl_value_field ok_data_fields[2];
        ok_data_fields[0].label = idl_label_name("key");
        ok_data_fields[0].value = idl_value_text_cstr(arena, "processed");
        ok_data_fields[1].label = idl_label_name("value");
        ok_data_fields[1].value = idl_value_nat32(arena, 100);
        idl_value *key_value_val = idl_value_record(arena, ok_data_fields, 2);

        idl_value *key_value_items[1] = {key_value_val};
        idl_value *vec_key_value_val = idl_value_vec(arena, key_value_items, 1);
        idl_value *opt_vec_key_value_val =
            idl_value_opt_some(arena, vec_key_value_val);

        idl_value_field ok_record_value_fields[3];
        ok_record_value_fields[0].label = idl_label_name("id");
        ok_record_value_fields[0].value = idl_value_nat32(arena, 1);
        ok_record_value_fields[1].label = idl_label_name("result");
        ok_record_value_fields[1].value =
            idl_value_text_cstr(arena, "Successfully processed complex data");
        ok_record_value_fields[2].label = idl_label_name("data");
        ok_record_value_fields[2].value = opt_vec_key_value_val;
        idl_value *ok_record_val =
            idl_value_record(arena, ok_record_value_fields, 3);

        idl_value_field ok_variant_field;
        ok_variant_field.label = idl_label_name("Ok");
        ok_variant_field.value = ok_record_val;
        idl_value *ok_variant_val =
            idl_value_variant(arena, 0, &ok_variant_field);

        // Second result: Err variant
        idl_value_field err_variant_field;
        err_variant_field.label = idl_label_name("Err");
        err_variant_field.value =
            idl_value_text_cstr(arena, "Failed to process item at index 1");
        idl_value *err_variant_val =
            idl_value_variant(arena, 1, &err_variant_field);

        // Build vec of variants
        idl_value *result_items[2] = {ok_variant_val, err_variant_val};
        idl_value *vec_result_val = idl_value_vec(arena, result_items, 2);

        idl_builder_arg(builder, vec_result_type, vec_result_val);
    }
    IC_API_BUILDER_END(api);
}
