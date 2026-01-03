#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

// #include <stdio.h>
#include <string.h>

#include "idl/candid.h"

// -----------------------------------------------------------------------------
// Helpers: small, in-memory demo data + reply helper using the low-level
// Candid builder so we can return records/variants/vec/opt.
// Note: Now using ic_api_reply_builder which handles everything automatically
// -----------------------------------------------------------------------------

// Build Address type/value
static idl_type *build_address_type(idl_arena *arena) {
    idl_field fields[3];

    fields[0].label.kind = IDL_LABEL_NAME;
    fields[0].label.id = idl_hash("street");
    fields[0].label.name = "street";
    fields[0].type = idl_type_text(arena);

    fields[1].label.kind = IDL_LABEL_NAME;
    fields[1].label.id = idl_hash("city");
    fields[1].label.name = "city";
    fields[1].type = idl_type_text(arena);

    fields[2].label.kind = IDL_LABEL_NAME;
    fields[2].label.id = idl_hash("zip");
    fields[2].label.name = "zip";
    fields[2].type = idl_type_nat(arena);

    return idl_type_record(arena, fields, 3);
}

static idl_value *build_address_value(idl_arena  *arena,
                                      const char *street,
                                      const char *city,
                                      uint32_t    zip) {
    idl_value_field fields[3];

    fields[0].label.kind = IDL_LABEL_NAME;
    fields[0].label.id = idl_hash("street");
    fields[0].label.name = "street";
    fields[0].value = idl_value_text_cstr(arena, street);

    fields[1].label.kind = IDL_LABEL_NAME;
    fields[1].label.id = idl_hash("city");
    fields[1].label.name = "city";
    fields[1].value = idl_value_text_cstr(arena, city);

    fields[2].label.kind = IDL_LABEL_NAME;
    fields[2].label.id = idl_hash("zip");
    fields[2].label.name = "zip";
    fields[2].value = idl_value_nat32(arena, zip);

    return idl_value_record(arena, fields, 3);
}

// Build Status type/value helpers
static idl_type *build_status_type(idl_arena *arena) {
    idl_field variants[3];

    variants[0].label.kind = IDL_LABEL_NAME;
    variants[0].label.id = idl_hash("Active");
    variants[0].label.name = "Active";
    variants[0].type = idl_type_null(arena);

    variants[1].label.kind = IDL_LABEL_NAME;
    variants[1].label.id = idl_hash("Inactive");
    variants[1].label.name = "Inactive";
    variants[1].type = idl_type_null(arena);

    variants[2].label.kind = IDL_LABEL_NAME;
    variants[2].label.id = idl_hash("Banned");
    variants[2].label.name = "Banned";
    variants[2].type = idl_type_text(arena);

    return idl_type_variant(arena, variants, 3);
}

static idl_value *build_status_value_active(idl_arena *arena) {
    idl_value_field field;
    field.label.kind = IDL_LABEL_NAME;
    field.label.id = idl_hash("Active");
    field.label.name = "Active";
    field.value = idl_value_null(arena);
    return idl_value_variant(arena, 0, &field);
}

static idl_value *build_status_value_banned(idl_arena *arena, const char *msg) {
    idl_value_field field;
    field.label.kind = IDL_LABEL_NAME;
    field.label.id = idl_hash("Banned");
    field.label.name = "Banned";
    field.value = idl_value_text_cstr(arena, msg);
    return idl_value_variant(arena, 2, &field);
}

// Build Profile type/value
static idl_type *build_profile_type(idl_arena *arena, idl_type *status_type) {
    idl_field fields[5];

    fields[0].label.kind = IDL_LABEL_NAME;
    fields[0].label.id = idl_hash("id");
    fields[0].label.name = "id";
    fields[0].type = idl_type_nat(arena);

    fields[1].label.kind = IDL_LABEL_NAME;
    fields[1].label.id = idl_hash("name");
    fields[1].label.name = "name";
    fields[1].type = idl_type_text(arena);

    fields[2].label.kind = IDL_LABEL_NAME;
    fields[2].label.id = idl_hash("emails");
    fields[2].label.name = "emails";
    fields[2].type = idl_type_vec(arena, idl_type_text(arena));

    fields[3].label.kind = IDL_LABEL_NAME;
    fields[3].label.id = idl_hash("age");
    fields[3].label.name = "age";
    fields[3].type = idl_type_opt(arena, idl_type_nat(arena));

    fields[4].label.kind = IDL_LABEL_NAME;
    fields[4].label.id = idl_hash("status");
    fields[4].label.name = "status";
    fields[4].type = status_type;

    return idl_type_record(arena, fields, 5);
}

static idl_value *build_profile_value(idl_arena *arena, idl_value *status_val) {
    idl_value_field fields[5];

    fields[0].label.kind = IDL_LABEL_NAME;
    fields[0].label.id = idl_hash("id");
    fields[0].label.name = "id";
    fields[0].value = idl_value_nat32(arena, 7);

    fields[1].label.kind = IDL_LABEL_NAME;
    fields[1].label.id = idl_hash("name");
    fields[1].label.name = "name";
    fields[1].value = idl_value_text_cstr(arena, "Dfinity Dev");

    idl_value *email_items[2];
    email_items[0] = idl_value_text_cstr(arena, "dev@example.com");
    email_items[1] = idl_value_text_cstr(arena, "contact@example.com");
    fields[2].label.kind = IDL_LABEL_NAME;
    fields[2].label.id = idl_hash("emails");
    fields[2].label.name = "emails";
    fields[2].value = idl_value_vec(arena, email_items, 2);

    fields[3].label.kind = IDL_LABEL_NAME;
    fields[3].label.id = idl_hash("age");
    fields[3].label.name = "age";
    fields[3].value = idl_value_opt_some(arena, idl_value_nat32(arena, 30));

    fields[4].label.kind = IDL_LABEL_NAME;
    fields[4].label.id = idl_hash("status");
    fields[4].label.name = "status";
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

    char reply[128];
    strcpy(reply, "Hello, ");
    strcat(reply, name);
    strcat(reply, "!");
    IC_API_REPLY_TEXT(reply);
}

// -----------------------------------------------------------------------------
// Update: add_user (text, nat, bool) -> ()
// Uses simplified argument parsing API
// -----------------------------------------------------------------------------
IC_API_UPDATE(add_user, "(text, nat, bool) -> ()") {
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
    ic_api_debug_print("add_user called with parsed arguments");
    ic_api_to_wire_empty(api);
}

// -----------------------------------------------------------------------------
// Update: set_address (name : text, addr : Address) -> ()
// -----------------------------------------------------------------------------
IC_API_UPDATE(
    set_address,
    "(text, record { street : text; city : text; zip : nat }) -> ()") {
    ic_api_debug_print("set_address called (demo only, no storage)");
    IC_API_REPLY_EMPTY();
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
    IC_API_BUILDER_BEGIN(api) {
        idl_type  *address_type = build_address_type(arena);
        idl_type  *opt_addr = idl_type_opt(arena, address_type);
        idl_value *addr_val = idl_value_opt_some(
            arena, build_address_value(arena, "1 Hacker Way", "IC", 2050));

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
              "(text, variant { Active; Inactive; Banned : text }) -> ()") {
    // Parse text argument using simplified API
    char *user = NULL;
    IC_API_PARSE_SIMPLE(api, text, user);

    // TODO: Variant parsing would need additional API support
    // For now, we just parse the text argument
    ic_api_debug_print("set_status called (demo only, no storage)");
    IC_API_REPLY_EMPTY();
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
    IC_API_BUILDER_BEGIN(api) {
        idl_type  *status_type = build_status_type(arena);
        idl_value *status_val =
            build_status_value_banned(arena, "too many requests");

        idl_builder_arg(builder, status_type, status_val);
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
    IC_API_BUILDER_BEGIN(api) {
        idl_type *status_type = build_status_type(arena);
        idl_type *profile_type = build_profile_type(arena, status_type);

        idl_field result_fields[2];
        result_fields[0].label.kind = IDL_LABEL_NAME;
        result_fields[0].label.id = idl_hash("Ok");
        result_fields[0].label.name = "Ok";
        result_fields[0].type = profile_type;
        result_fields[1].label.kind = IDL_LABEL_NAME;
        result_fields[1].label.id = idl_hash("Err");
        result_fields[1].label.name = "Err";
        result_fields[1].type = idl_type_text(arena);

        idl_type *result_variant = idl_type_variant(arena, result_fields, 2);
        idl_type *vec_result = idl_type_vec(arena, result_variant);

        idl_value_field ok_field;
        ok_field.label = result_fields[0].label;
        ok_field.value =
            build_profile_value(arena, build_status_value_active(arena));
        idl_value *ok_variant = idl_value_variant(arena, 0, &ok_field);

        idl_value_field err_field;
        err_field.label = result_fields[1].label;
        err_field.value = idl_value_text_cstr(arena, "duplicate id");
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
    IC_API_BUILDER_BEGIN(api) {
        idl_type  *status_type = build_status_type(arena);
        idl_type  *profile_type = build_profile_type(arena, status_type);
        idl_type  *opt_profile = idl_type_opt(arena, profile_type);
        idl_value *profile_val = idl_value_opt_some(
            arena, build_profile_value(arena, build_status_value_active(arena)));

        idl_builder_arg(builder, opt_profile, profile_val);
    }
    IC_API_BUILDER_END(api);
}

// -----------------------------------------------------------------------------
// Query: stats () -> (nat, vec nat, text)
// -----------------------------------------------------------------------------
IC_API_QUERY(stats, "() -> (nat, vec nat, text)") {
    // Build reply using simplified API (arena is automatically managed)
    IC_API_BUILDER_BEGIN(api) {
        idl_builder_arg_nat64(builder, 3); // count

        uint64_t   numbers_raw[3] = {1, 2, 3};
        idl_value *num_vals[3];
        num_vals[0] = idl_value_nat64(arena, numbers_raw[0]);
        num_vals[1] = idl_value_nat64(arena, numbers_raw[1]);
        num_vals[2] = idl_value_nat64(arena, numbers_raw[2]);

        idl_type  *vec_nat_type = idl_type_vec(arena, idl_type_nat(arena));
        idl_value *vec_val = idl_value_vec(arena, num_vals, 3);
        idl_builder_arg(builder, vec_nat_type, vec_val);

        idl_builder_arg_text_cstr(builder, "ok");
    }
    IC_API_BUILDER_END(api);
}
