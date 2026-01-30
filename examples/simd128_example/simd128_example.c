#include "ic_c_sdk.h"
#include <stdint.h>
#include <wasm_simd128.h>

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

#include "ic_candid_builder.h"
#include "idl/candid.h"

// SIMD128 vector addition: out[i] = a[i] + b[i] for i=0..3
// Using restrict + intrinsics (most reliable approach)
static void add4_i32_simd(int32_t *restrict out,
                          const int32_t *restrict a,
                          const int32_t *restrict b) {
    // Assume pointers are aligned to 16 bytes (required for simd128)
    (void)__builtin_assume_aligned(out, 16);
    (void)__builtin_assume_aligned(a, 16);
    (void)__builtin_assume_aligned(b, 16);

    // Load 4x i32 into v128 vectors
    v128_t va = wasm_v128_load(a);
    v128_t vb = wasm_v128_load(b);

    // Lane-wise SIMD addition
    v128_t vc = wasm_i32x4_add(va, vb);

    // Store result back
    wasm_v128_store(out, vc);
}

// Query: add_vectors - SIMD128 vector addition (no input, uses fixed test data)
// Returns vec int32[4] with result
IC_API_QUERY(add_vectors, "() -> (vec int32)") {
    // Fixed test data: a = [1, 2, 3, 4], b = [10, 20, 30, 40]
    // Expected result: [11, 22, 33, 44]
    int32_t a[4] = {1, 2, 3, 4};
    int32_t b[4] = {10, 20, 30, 40};
    int32_t result[4];

    add4_i32_simd(result, a, b);

    IC_API_BUILDER_BEGIN(api) {
        idl_value *vec_val =
            IDL_VEC(arena, V_INT32(result[0]), V_INT32(result[1]),
                    V_INT32(result[2]), V_INT32(result[3]));
        idl_type *vec_type = T_VEC(T_INT32);
        idl_builder_arg(builder, vec_type, vec_val);
    }
    IC_API_BUILDER_END(api);
}

// Query: greet - Simple greeting
IC_API_QUERY(greet, "() -> (text)") {
    IC_API_REPLY_TEXT(
        "SIMD128 Example Canister - Call add_vectors() to see SIMD result");
}
