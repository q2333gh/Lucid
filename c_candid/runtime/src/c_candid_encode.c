/**
 * c_candid_encode - Pure C Candid encoder using the idl runtime.
 *
 * Reads Candid text from stdin, outputs hex-encoded DIDL to stdout.
 *
 * Currently supports a simplified subset of Candid text syntax:
 *   - Primitives: true, false, null, integers, floats, "text"
 *   - Tuples: (val1, val2, ...)
 *
 * Usage: echo '("hello", 42)' | ./c_candid_encode
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idl/runtime.h"

#define MAX_INPUT 65536

/* Simple text parser state */
typedef struct {
    const char *input;
    size_t      pos;
    size_t      len;
    idl_arena  *arena;
} parser_t;

static void skip_whitespace(parser_t *p) {
    while (p->pos < p->len && isspace((unsigned char)p->input[p->pos])) {
        p->pos++;
    }
}

static int peek(parser_t *p) {
    skip_whitespace(p);
    return (p->pos < p->len) ? p->input[p->pos] : EOF;
}

static int consume(parser_t *p) {
    skip_whitespace(p);
    return (p->pos < p->len) ? p->input[p->pos++] : EOF;
}

static int match(parser_t *p, const char *s) {
    skip_whitespace(p);
    size_t len = strlen(s);
    if (p->pos + len <= p->len && strncmp(p->input + p->pos, s, len) == 0) {
        p->pos += len;
        return 1;
    }
    return 0;
}

/* Forward declaration */
static int parse_value(parser_t *p, idl_builder *builder);

/* Parse a string literal */
static int parse_string(parser_t *p, idl_builder *builder) {
    if (consume(p) != '"') {
        return 0;
    }

    char   buf[4096];
    size_t len = 0;

    while (p->pos < p->len && p->input[p->pos] != '"') {
        if (p->input[p->pos] == '\\' && p->pos + 1 < p->len) {
            p->pos++;
            switch (p->input[p->pos]) {
            case 'n':
                buf[len++] = '\n';
                break;
            case 't':
                buf[len++] = '\t';
                break;
            case 'r':
                buf[len++] = '\r';
                break;
            case '\\':
                buf[len++] = '\\';
                break;
            case '"':
                buf[len++] = '"';
                break;
            default:
                buf[len++] = p->input[p->pos];
                break;
            }
        } else {
            buf[len++] = p->input[p->pos];
        }
        p->pos++;
        if (len >= sizeof(buf) - 1) {
            break;
        }
    }

    if (p->pos >= p->len || p->input[p->pos] != '"') {
        fprintf(stderr, "Unterminated string\n");
        return 0;
    }
    p->pos++; /* consume closing quote */

    buf[len] = '\0';
    return idl_builder_arg_text(builder, buf, len) == IDL_STATUS_OK;
}

/* Parse a number (int or float) */
static int parse_number(parser_t *p, idl_builder *builder) {
    skip_whitespace(p);

    char   buf[64];
    size_t len = 0;
    int    is_negative = 0;
    int    has_dot = 0;
    int    has_underscore = 0;

    if (p->input[p->pos] == '-' || p->input[p->pos] == '+') {
        is_negative = (p->input[p->pos] == '-');
        buf[len++] = p->input[p->pos++];
    }

    while (p->pos < p->len && len < sizeof(buf) - 1) {
        char c = p->input[p->pos];
        if (isdigit((unsigned char)c)) {
            buf[len++] = c;
            p->pos++;
        } else if (c == '_') {
            has_underscore = 1;
            p->pos++; /* skip underscores */
        } else if (c == '.' && !has_dot) {
            has_dot = 1;
            buf[len++] = c;
            p->pos++;
        } else if ((c == 'e' || c == 'E') && has_dot) {
            buf[len++] = c;
            p->pos++;
            if (p->pos < p->len &&
                (p->input[p->pos] == '+' || p->input[p->pos] == '-')) {
                buf[len++] = p->input[p->pos++];
            }
        } else {
            break;
        }
    }

    buf[len] = '\0';
    (void)has_underscore;

    if (has_dot) {
        double val = strtod(buf, NULL);
        return idl_builder_arg_float64(builder, val) == IDL_STATUS_OK;
    } else {
        /* Parse as int - use int type for Candid */
        int64_t val = strtoll(buf, NULL, 10);

        /* Create int type and value manually */
        idl_type  *t_int = idl_type_int(builder->arena);
        idl_value *v_int = idl_value_int_i64(builder->arena, val);
        if (!t_int || !v_int) {
            return 0;
        }
        return idl_builder_arg(builder, t_int, v_int) == IDL_STATUS_OK;
    }
}

/* Parse a single value */
static int parse_value(parser_t *p, idl_builder *builder) {
    int c = peek(p);

    if (c == '"') {
        return parse_string(p, builder);
    }

    if (c == '-' || c == '+' || isdigit((unsigned char)c)) {
        return parse_number(p, builder);
    }

    if (match(p, "true")) {
        return idl_builder_arg_bool(builder, 1) == IDL_STATUS_OK;
    }

    if (match(p, "false")) {
        return idl_builder_arg_bool(builder, 0) == IDL_STATUS_OK;
    }

    if (match(p, "null")) {
        return idl_builder_arg_null(builder) == IDL_STATUS_OK;
    }

    fprintf(stderr, "Unexpected character at position %zu: '%c'\n", p->pos,
            c == EOF ? '?' : (char)c);
    return 0;
}

/* Parse tuple: (val1, val2, ...) */
static int parse_tuple(parser_t *p, idl_builder *builder) {
    if (consume(p) != '(') {
        fprintf(stderr, "Expected '('\n");
        return 0;
    }

    while (1) {
        if (peek(p) == ')') {
            consume(p);
            break;
        }

        if (!parse_value(p, builder)) {
            return 0;
        }

        int c = peek(p);
        if (c == ',') {
            consume(p);
        } else if (c == ')') {
            consume(p);
            break;
        } else {
            fprintf(stderr, "Expected ',' or ')'\n");
            return 0;
        }
    }

    return 1;
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

    /* Trim trailing whitespace */
    while (total > 0 && isspace((unsigned char)input[total - 1])) {
        input[--total] = '\0';
    }

    /* Initialize arena and builder */
    idl_arena arena;
    if (idl_arena_init(&arena, 65536) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to initialize arena\n");
        return 1;
    }

    idl_builder builder;
    if (idl_builder_init(&builder, &arena) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to initialize builder\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    /* Parse input */
    parser_t parser = {.input = input, .pos = 0, .len = total, .arena = &arena};

    if (peek(&parser) == '(') {
        if (!parse_tuple(&parser, &builder)) {
            idl_arena_destroy(&arena);
            return 1;
        }
    } else {
        /* Single value */
        if (!parse_value(&parser, &builder)) {
            idl_arena_destroy(&arena);
            return 1;
        }
    }

    /* Serialize and output hex */
    char  *hex;
    size_t hex_len;
    if (idl_builder_serialize_hex(&builder, &hex, &hex_len) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to serialize\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    printf("%s\n", hex);

    idl_arena_destroy(&arena);
    return 0;
}
