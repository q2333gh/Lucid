#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

#include "idl/candid.h"
#include <string.h>
#include <tinyprintf.h>
// Minimal Hello World
IC_API_QUERY(greet, "() -> (text)") {
    IC_API_REPLY_TEXT("Hello from minimal C canister!");
}
// Converts an integer value to a string.
// value: the integer to convert
// str: output buffer, must be large enough (12 bytes is enough for int32)
// Returns: pointer to the output string (str)
char *int_to_str(int value, char *str) {
    char *p = str;
    char *start = str;
    int   negative = 0;

    if (value < 0) {
        negative = 1;
        // Handle the minimum negative value for int32: -2147483648
        if (value == -2147483648) {
            *p++ = '-';
            *p++ = '2';
            value = 147483648;
        } else {
            value = -value;
        }
    }

    // Generate digits in reverse order
    char *digits_start = p;
    do {
        *p++ = '0' + (value % 10);
        value /= 10;
    } while (value);

    if (negative && digits_start == str) {
        *p++ = '-';
    }

    *p = '\0';

    // Reverse the digits part to get the correct order
    char *left = negative ? digits_start : str;
    char *right = p - 1;
    while (left < right) {
        char tmp = *left;
        *left = *right;
        *right = tmp;
        left++;
        right--;
    }

    return str;
}

static unsigned int counter = 0;

IC_API_UPDATE(increment, "() -> (text)") {
    counter++;
    char msg[128] = {0};
    tfp_snprintf(msg, sizeof msg, "increment called, result is: %u", counter);
    ic_api_debug_print(msg);

    char reply[64] = {0};
    tfp_snprintf(reply, sizeof reply, "Incremented! value=%u", counter);
    IC_API_REPLY_TEXT(reply);
}