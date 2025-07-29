#include "../include/lucid_wasm/api.h"
#include <stdio.h>
#include <assert.h>

int main() {
    // Test basic addition
    assert(add(5, 3) == 8);
    assert(add(10, 20) == 30);
    assert(add(100, 200) == 300);
    assert(add(-5, 8) == 3);
    assert(add(0, 0) == 0);
    
    printf("All tests passed!\n");
    return 0;
}