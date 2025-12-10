#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

#include "idl/candid.h"
#include <math.h>
#include <string.h>
#include <time.h>

// Helper: convert integer to string (supports only base 10, simple version)
static void int64_to_str(int64_t val, char *out, size_t out_len) {
    char buf[32];
    int pos = 0;
    int is_negative = 0;
    if (val < 0) {
        is_negative = 1;
        val = -val;
    }
    do {
        buf[pos++] = (char)('0' + (val % 10));
        val /= 10;
    } while(val && pos < (int)(sizeof(buf) - 1));
    if (is_negative && pos < (int)(sizeof(buf) - 1)) {
        buf[pos++] = '-';
    }
    buf[pos] = '\0';

    // Reverse and copy to out
    size_t used = 0;
    for (int i = pos - 1; i >= 0 && used + 1 < out_len; --i) {
        out[used++] = buf[i];
    }
    out[used] = '\0';
}

// Helper: add zero-padded number to string buffer (for date/time formatting)
static void append_padded(char *out, size_t out_len, int val, int width) {
    char tmp[8];
    int num_len = 0;
    int v = val;
    do {
        num_len++;
        v /= 10;
    } while(v);
    int pad = width - num_len;
    size_t len = strlen(out);
    for(int i=0; i<pad && (len+1)<out_len; ++i) {
        out[len++] = '0';
    }
    int64_to_str(val, tmp, sizeof(tmp));
    strncat(out, tmp, out_len - strlen(out) - 1);
}

// Helper: Format int64_t time (nanoseconds since epoch) into human readable "YYYY-MM-DD HH:MM:SS"
static void format_human_time(int64_t t_ns, char *out, size_t out_len) {
    // Convert ns to seconds
    time_t secs = (time_t)(t_ns / 1000000000);
    struct tm tm_val;
#if defined(__wasi__)
    gmtime_r(&secs, &tm_val);
#else
    // fallback, may not be threadsafe on all platforms
    memcpy(&tm_val, gmtime(&secs), sizeof(tm_val));
#endif
    // Fallback if gmtime fails: just print seconds
    if (tm_val.tm_year < 70 || tm_val.tm_year > 200) {
        // Print as integer seconds (no snprintf)
        int64_to_str((long long)secs, out, out_len);
    } else {
        // Format: YYYY-MM-DD HH:MM:SS without snprintf
        out[0] = '\0';
        append_padded(out, out_len, tm_val.tm_year + 1900, 4); // YYYY
        strncat(out, "-", out_len - strlen(out) - 1);

        append_padded(out, out_len, tm_val.tm_mon + 1, 2); // MM
        strncat(out, "-", out_len - strlen(out) - 1);

        append_padded(out, out_len, tm_val.tm_mday, 2); // DD
        strncat(out, " ", out_len - strlen(out) - 1);

        append_padded(out, out_len, tm_val.tm_hour, 2); // HH
        strncat(out, ":", out_len - strlen(out) - 1);

        append_padded(out, out_len, tm_val.tm_min, 2); // MM
        strncat(out, ":", out_len - strlen(out) - 1);

        append_padded(out, out_len, tm_val.tm_sec, 2); // SS
    }
}

IC_API_QUERY(greet, "() -> (text)") {
    int64_t t = ic_api_time();
    char t_str[64] = {0};
    int64_to_str((long long)t, t_str, sizeof(t_str));
    ic_api_debug_print(t_str);
    char human[64] = {0};
    format_human_time(t, human, sizeof(human));

    // Build reply string with strcat
    char buf[128] = {0};
    strcat(buf, "Hello from minimal C canister! Time: ");
    strcat(buf, human);

    ic_api_debug_print(buf);
    IC_API_REPLY_TEXT(buf);

    if (t == 0) {
        ic_api_trap("Time is zero?");
    }
}
