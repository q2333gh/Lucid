#include <stdio.h>
#include <stdlib.h>

#include "ic0_mock.h"
#include "ic_candid.h"

void greet(void);
void increment(void);

static void print_reply_text(const char *label) {
    uint8_t *data = NULL;
    size_t   len = 0;
    ic0_mock_take_reply(&data, &len);
    if (data == NULL || len == 0) {
        printf("%s: <no reply>\n", label);
        free(data);
        return;
    }

    size_t offset = 0;
    char  *text = NULL;
    size_t text_len = 0;
    if (candid_deserialize_text(data, len, &offset, &text, &text_len) ==
        IC_OK) {
        printf("%s: %s\n", label, text);
        free(text);
    } else {
        printf("%s: <reply len=%zu>\n", label, len);
    }

    free(data);
}

int main(void) {
    ic0_mock_reset();

    ic0_mock_set_arg_data(NULL, 0);
    ic0_mock_clear_reply();
    greet();
    print_reply_text("greet");

    ic0_mock_set_arg_data(NULL, 0);
    ic0_mock_clear_reply();
    increment();
    print_reply_text("increment #1");

    ic0_mock_set_arg_data(NULL, 0);
    ic0_mock_clear_reply();
    increment();
    print_reply_text("increment #2");

    return 0;
}
