/**
 * c_candid_decode - Pure C Candid decoder using the idl runtime.
 *
 * Reads hex-encoded DIDL from stdin, outputs Candid text to stdout.
 *
 * Usage: echo "4449444c0001710568656c6c6f" | ./c_candid_decode
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idl/runtime.h"

#define MAX_INPUT 65536

/* Convert hex string to bytes */
static int
hex_to_bytes(const char *hex, size_t hex_len, uint8_t *out, size_t *out_len) {
    size_t j = 0;
    for (size_t i = 0; i < hex_len; i += 2) {
        if (i + 1 >= hex_len) {
            return 0; /* Odd number of hex chars */
        }
        char c1 = hex[i];
        char c2 = hex[i + 1];

        int v1, v2;
        if (c1 >= '0' && c1 <= '9')
            v1 = c1 - '0';
        else if (c1 >= 'a' && c1 <= 'f')
            v1 = c1 - 'a' + 10;
        else if (c1 >= 'A' && c1 <= 'F')
            v1 = c1 - 'A' + 10;
        else
            return 0;

        if (c2 >= '0' && c2 <= '9')
            v2 = c2 - '0';
        else if (c2 >= 'a' && c2 <= 'f')
            v2 = c2 - 'a' + 10;
        else if (c2 >= 'A' && c2 <= 'F')
            v2 = c2 - 'A' + 10;
        else
            return 0;

        out[j++] = (uint8_t)((v1 << 4) | v2);
    }
    *out_len = j;
    return 1;
}

/* Print a value as Candid text */
static void print_value(const idl_value *v, int in_tuple) {
    if (!v) {
        printf("null");
        return;
    }

    switch (v->kind) {
    case IDL_VALUE_NULL:
        printf("null : null");
        break;

    case IDL_VALUE_BOOL:
        printf("%s", v->data.bool_val ? "true" : "false");
        break;

    case IDL_VALUE_NAT8:
        printf("%u : nat8", v->data.nat8_val);
        break;

    case IDL_VALUE_NAT16:
        printf("%u : nat16", v->data.nat16_val);
        break;

    case IDL_VALUE_NAT32:
        printf("%u : nat32", v->data.nat32_val);
        break;

    case IDL_VALUE_NAT64:
        printf("%lu : nat64", (unsigned long)v->data.nat64_val);
        break;

    case IDL_VALUE_NAT:
        /* Decode NAT from LEB128 bytes */
        if (v->data.bignum.len > 0) {
            uint64_t val = 0;
            size_t   consumed = 0;
            if (idl_uleb128_decode(v->data.bignum.data, v->data.bignum.len,
                                   &consumed, &val) == IDL_STATUS_OK) {
                printf("%lu", (unsigned long)val);
            } else {
                printf("<nat>");
            }
        } else {
            printf("0");
        }
        break;

    case IDL_VALUE_INT8:
        printf("%d : int8", v->data.int8_val);
        break;

    case IDL_VALUE_INT16:
        printf("%d : int16", v->data.int16_val);
        break;

    case IDL_VALUE_INT32:
        printf("%d : int32", v->data.int32_val);
        break;

    case IDL_VALUE_INT64:
        printf("%ld : int64", (long)v->data.int64_val);
        break;

    case IDL_VALUE_INT:
        /* Decode INT from SLEB128 bytes */
        if (v->data.bignum.len > 0) {
            int64_t val = 0;
            size_t  consumed = 0;
            if (idl_sleb128_decode(v->data.bignum.data, v->data.bignum.len,
                                   &consumed, &val) == IDL_STATUS_OK) {
                printf("%ld : int", (long)val);
            } else {
                printf("<int>");
            }
        } else {
            printf("0 : int");
        }
        break;

    case IDL_VALUE_FLOAT32:
        printf("%g : float32", (double)v->data.float32_val);
        break;

    case IDL_VALUE_FLOAT64:
        printf("%g", v->data.float64_val);
        break;

    case IDL_VALUE_TEXT:
        printf("\"");
        for (size_t i = 0; i < v->data.text.len; i++) {
            char c = v->data.text.data[i];
            switch (c) {
            case '\n':
                printf("\\n");
                break;
            case '\r':
                printf("\\r");
                break;
            case '\t':
                printf("\\t");
                break;
            case '"':
                printf("\\\"");
                break;
            case '\\':
                printf("\\\\");
                break;
            default:
                if (c >= 32 && c < 127) {
                    putchar(c);
                } else {
                    printf("\\%02x", (unsigned char)c);
                }
                break;
            }
        }
        printf("\"");
        break;

    case IDL_VALUE_RESERVED:
        printf("reserved");
        break;

    case IDL_VALUE_PRINCIPAL:
        /* TODO: proper principal encoding */
        printf("principal \"");
        for (size_t i = 0; i < v->data.principal.len; i++) {
            printf("%02x", v->data.principal.data[i]);
        }
        printf("\"");
        break;

    case IDL_VALUE_BLOB:
        printf("blob \"");
        for (size_t i = 0; i < v->data.blob.len; i++) {
            printf("\\%02x", v->data.blob.data[i]);
        }
        printf("\"");
        break;

    case IDL_VALUE_OPT:
        if (v->data.opt) {
            printf("opt ");
            print_value(v->data.opt, 0);
        } else {
            printf("null");
        }
        break;

    case IDL_VALUE_VEC:
        printf("vec { ");
        for (size_t i = 0; i < v->data.vec.len; i++) {
            if (i > 0) {
                printf("; ");
            }
            print_value(v->data.vec.items[i], 0);
        }
        printf(" }");
        break;

    case IDL_VALUE_RECORD:
        printf("record { ");
        for (size_t i = 0; i < v->data.record.len; i++) {
            if (i > 0) {
                printf("; ");
            }
            idl_value_field *f = &v->data.record.fields[i];
            if (f->label.kind == IDL_LABEL_NAME && f->label.name) {
                printf("%s = ", f->label.name);
            } else {
                printf("%lu = ", (unsigned long)f->label.id);
            }
            print_value(f->value, 0);
        }
        printf(" }");
        break;

    case IDL_VALUE_VARIANT:
        printf("variant { ");
        if (v->data.record.len > 0) {
            idl_value_field *f = &v->data.record.fields[0];
            if (f->label.kind == IDL_LABEL_NAME && f->label.name) {
                printf("%s = ", f->label.name);
            } else {
                printf("%lu = ", (unsigned long)f->label.id);
            }
            print_value(f->value, 0);
        }
        printf(" }");
        break;
    }
    (void)in_tuple;
}

int main(void) {
    /* Read input from stdin */
    char   input[MAX_INPUT];
    size_t total = 0;

    while (total < sizeof(input) - 1) {
        size_t n = fread(input + total, 1, sizeof(input) - 1 - total, stdin);
        if (n == 0) {
            break;
        }
        total += n;
    }
    input[total] = '\0';

    /* Trim whitespace and skip optional "0x" prefix */
    char *hex = input;
    while (*hex && isspace((unsigned char)*hex)) {
        hex++;
    }
    if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex += 2;
    }

    /* Remove trailing whitespace */
    size_t hex_len = strlen(hex);
    while (hex_len > 0 && isspace((unsigned char)hex[hex_len - 1])) {
        hex_len--;
    }

    /* Convert hex to bytes */
    uint8_t bytes[MAX_INPUT / 2];
    size_t  bytes_len = 0;
    if (!hex_to_bytes(hex, hex_len, bytes, &bytes_len)) {
        fprintf(stderr, "Invalid hex input\n");
        return 1;
    }

    /* Initialize arena and deserializer */
    idl_arena arena;
    if (idl_arena_init(&arena, 65536) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to initialize arena\n");
        return 1;
    }

    idl_deserializer *de = NULL;
    if (idl_deserializer_new(bytes, bytes_len, &arena, &de) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to parse DIDL header\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    /* Read all values */
    int first = 1;
    printf("(");
    while (!idl_deserializer_is_done(de)) {
        idl_type  *type = NULL;
        idl_value *val = NULL;
        if (idl_deserializer_get_value(de, &type, &val) != IDL_STATUS_OK) {
            fprintf(stderr, "Failed to decode value\n");
            idl_arena_destroy(&arena);
            return 1;
        }

        if (!first) {
            printf(", ");
        }
        first = 0;
        print_value(val, 1);
    }
    printf(")\n");

    if (idl_deserializer_done(de) != IDL_STATUS_OK) {
        fprintf(stderr, "Trailing bytes in input\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    idl_arena_destroy(&arena);
    return 0;
}
