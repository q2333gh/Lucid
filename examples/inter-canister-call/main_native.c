#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ic0_mock.h"
#include "ic_candid.h"

void trigger_call(void);

static int handle_call(const ic0_mock_call_t    *call,
                       ic0_mock_call_response_t *out) {
    (void)call;

    ic_buffer_t buf;
    if (ic_buffer_init(&buf) != IC_OK) {
        return 1;
    }
    if (candid_serialize_text(&buf, "mock inter-canister reply") != IC_OK) {
        ic_buffer_free(&buf);
        return 1;
    }

    size_t   reply_len = buf.size;
    uint8_t *copy = malloc(reply_len);
    if (copy == NULL) {
        ic_buffer_free(&buf);
        return 1;
    }
    memcpy(copy, buf.data, reply_len);
    ic_buffer_free(&buf);

    out->action = IC0_MOCK_CALL_REPLY;
    out->data = copy;
    out->len = reply_len;
    out->data_owned = true;
    out->reject_msg_owned = false;
    return 0;
}

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
    ic0_mock_set_call_handler(handle_call);

    ic_buffer_t arg;
    if (ic_buffer_init(&arg) != IC_OK) {
        return 1;
    }
    if (candid_serialize_text(&arg, "2vxsx-fae,greet") != IC_OK) {
        ic_buffer_free(&arg);
        return 1;
    }

    ic0_mock_set_arg_data(arg.data, arg.size);
    ic0_mock_clear_reply();
    trigger_call();
    ic_buffer_free(&arg);

    print_reply_text("trigger_call");
    return 0;
}
