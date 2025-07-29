#ifndef LUCID_WASM_API_H
#define LUCID_WASM_API_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Add two integers
 * @param a First integer
 * @param b Second integer
 * @return Sum of a and b
 */
int add(int a, int b);

#ifdef __cplusplus
}
#endif

#endif // LUCID_WASM_API_H