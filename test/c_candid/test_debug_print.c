/**
 * Test debug printing (only works on non-WASM platforms)
 */
#include "idl/debug.h"
#include "idl/runtime.h"
#include <stdio.h>

int main(void) {
    printf("=== Testing c_candid debug printing ===\n\n");

#ifdef CANDID_DEBUG_PRINT_ENABLED
    printf("✓ CANDID_DEBUG_PRINT_ENABLED is defined\n");
    printf("Debug prints will appear on stderr with [c_candid] prefix:\n\n");

    IDL_DEBUG_PRINT("This is a debug message");
    IDL_DEBUG_PRINT("Value: %d, String: %s", 42, "test");

    uint8_t data[] = {0x44, 0x49, 0x44, 0x4c, 0x00, 0x00};
    IDL_DEBUG_HEX("Sample Candid", data, sizeof(data));

    printf("\n✓ Debug printing works!\n");
#else
    printf("✗ CANDID_DEBUG_PRINT_ENABLED is NOT defined\n");
    printf("Debug prints are disabled (expected for WASM builds)\n");
#endif

    return 0;
}
