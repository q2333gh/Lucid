#include <stdio.h>
#include <stdlib.h>

#include "ic0_mock.h"
#include "ic_candid.h"

void init(void);
void greet(void);
void get_counter(void);
void increment(void);
void pre_upgrade(void);
void post_upgrade(void);
void restore_and_debug(void);

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

static void print_reply_nat(const char *label) {
    uint8_t *data = NULL;
    size_t   len = 0;
    ic0_mock_take_reply(&data, &len);
    if (data == NULL || len == 0) {
        printf("%s: <no reply>\n", label);
        free(data);
        return;
    }

    size_t   offset = 0;
    uint64_t value = 0;
    if (candid_deserialize_nat(data, len, &offset, &value) == IC_OK) {
        printf("%s: %llu\n", label, (unsigned long long)value);
    } else {
        printf("%s: <reply len=%zu>\n", label, len);
    }

    free(data);
}

int main(void) {
    ic0_mock_reset();
    ic0_mock_stable_reset();

    init();

    ic0_mock_set_arg_data(NULL, 0);
    ic0_mock_clear_reply();
    greet();
    print_reply_text("greet");

    ic0_mock_set_arg_data(NULL, 0);
    ic0_mock_clear_reply();
    get_counter();
    print_reply_nat("get_counter");

    ic0_mock_set_arg_data(NULL, 0);
    ic0_mock_clear_reply();
    increment();
    print_reply_nat("increment");

    ic0_mock_set_arg_data(NULL, 0);
    pre_upgrade();
    post_upgrade();

    ic0_mock_set_arg_data(NULL, 0);
    ic0_mock_clear_reply();
    get_counter();
    print_reply_nat("get_counter after upgrade");

    ic0_mock_set_arg_data(NULL, 0);
    ic0_mock_clear_reply();
    restore_and_debug();
    print_reply_text("restore_and_debug");

    return 0;
}
