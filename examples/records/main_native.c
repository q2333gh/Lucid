// Minimal main function for native builds (size analysis only)
// This file is only used when building for native (not WASI)
// #include <stdio.h>

// Declare exported functions (they exist but we don't call them)
// This is just to ensure they're linked in for size analysis
extern void greet(void);
extern void add_user(void);
extern void get_address(void);
extern void get_profile(void);
extern void get_user_info(void);
extern void get_nested_data(void);
extern void get_optional_data(void);
extern void get_complex_record(void);

int main(void) {
    // For size analysis, we don't need to actually call these functions
    // They're already compiled and linked, which is what we want to measure
    // printf("Native build for size analysis - functions compiled but not "
    //  "executed\n");
    return 0;
}
