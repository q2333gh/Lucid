/**
 * Manual type construction for http_request_args
 * This avoids the limitations of idl_type_from_value with variants
 */
#include "ic_api.h"
#include "ic_candid_builder.h"
#include "ic_http_request.h"
#include "idl/candid.h"

/**
 * Build complete http_request_args type according to IC spec
 *
 * type http_request_args = record {
 *     url : text;
 *     max_response_bytes : opt nat64;
 *     method : variant { get; head; post };
 *     headers : vec http_header;
 *     body : opt blob;
 *     transform : opt record { function: func(...) query; context: blob };
 *     is_replicated : opt bool;
 * };
 */
idl_type *build_http_request_args_type(idl_arena *arena) {
    // Build http_header type first: record { name: text; value: text }
    idl_field *header_fields = idl_arena_alloc(arena, 2 * sizeof(idl_field));
    header_fields[0] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("name"),
                  .name = "name"},
        .type = idl_type_text(arena)
    };
    header_fields[1] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("value"),
                  .name = "value"},
        .type = idl_type_text(arena)
    };
    // Sort by label id
    if (header_fields[0].label.id > header_fields[1].label.id) {
        idl_field tmp = header_fields[0];
        header_fields[0] = header_fields[1];
        header_fields[1] = tmp;
    }
    idl_type *header_type = idl_type_record(arena, header_fields, 2);

    // Build method variant type: variant { get; head; post }
    // CRITICAL: Must be in alphabetical order
    idl_field *method_variants = idl_arena_alloc(arena, 3 * sizeof(idl_field));
    method_variants[0] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("get"), .name = "get"},
        .type = idl_type_null(arena)
    };
    method_variants[1] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("head"),
                  .name = "head"},
        .type = idl_type_null(arena)
    };
    method_variants[2] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("post"),
                  .name = "post"},
        .type = idl_type_null(arena)
    };
    idl_type *method_type = idl_type_variant(arena, method_variants, 3);

    // Build main record fields (use arena allocation)
    idl_field *args_fields = idl_arena_alloc(arena, 7 * sizeof(idl_field));
    int        field_idx = 0;

    // 1. url: text
    args_fields[field_idx++] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("url"), .name = "url"},
        .type = idl_type_text(arena)
    };

    // 2. max_response_bytes: opt nat64
    args_fields[field_idx++] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("max_response_bytes"),
                  .name = "max_response_bytes"},
        .type = idl_type_opt(arena, idl_type_nat64(arena))
    };

    // 3. method: variant { get; head; post }
    args_fields[field_idx++] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("method"),
                  .name = "method"},
        .type = method_type
    };

    // 4. headers: vec http_header
    args_fields[field_idx++] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("headers"),
                  .name = "headers"},
        .type = idl_type_vec(arena, header_type)
    };

    // 5. body: opt blob
    args_fields[field_idx++] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("body"),
                  .name = "body"},
        .type = idl_type_opt(arena, idl_type_vec(arena, idl_type_nat8(arena)))
    };

    // 6. transform: opt record { function, context }
    // For now, use opt null as we don't use transform
    args_fields[field_idx++] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("transform"),
                  .name = "transform"},
        .type = idl_type_opt(arena, idl_type_null(arena))
    };

    // 7. is_replicated: opt bool
    args_fields[field_idx++] = (idl_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("is_replicated"),
                  .name = "is_replicated"},
        .type = idl_type_opt(arena, idl_type_bool(arena))
    };

    // Sort fields by label id (CRITICAL for Candid)
    for (int i = 0; i < field_idx - 1; i++) {
        for (int j = i + 1; j < field_idx; j++) {
            if (args_fields[i].label.id > args_fields[j].label.id) {
                idl_field tmp = args_fields[i];
                args_fields[i] = args_fields[j];
                args_fields[j] = tmp;
            }
        }
    }

    // Debug output removed for compatibility with native tests

    return idl_type_record(arena, args_fields, field_idx);
}
