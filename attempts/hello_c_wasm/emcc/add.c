#include <emscripten.h>

// Simple add function that can be compiled to WASM
int add(int a, int b) {
    return a + b;
}

// Export the function for use in JavaScript
EMSCRIPTEN_KEEPALIVE
int add_exported(int a, int b) {
    return add(a, b);
}