#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "idl/runtime.h"

static void test_uleb128_roundtrip(uint64_t value) {
  uint8_t buf[16];
  size_t written = 0;
  uint64_t decoded = 0;
  size_t consumed = 0;
  assert(idl_uleb128_encode(value, buf, sizeof(buf), &written) ==
         IDL_STATUS_OK);
  assert(idl_uleb128_decode(buf, written, &consumed, &decoded) ==
         IDL_STATUS_OK);
  assert(consumed == written);
  assert(decoded == value);
}

static void test_sleb128_roundtrip(int64_t value) {
  uint8_t buf[16];
  size_t written = 0;
  int64_t decoded = 0;
  size_t consumed = 0;
  assert(idl_sleb128_encode(value, buf, sizeof(buf), &written) ==
         IDL_STATUS_OK);
  assert(idl_sleb128_decode(buf, written, &consumed, &decoded) ==
         IDL_STATUS_OK);
  assert(consumed == written);
  assert(decoded == value);
}

static void test_uleb128_overflow(void) {
  uint8_t buf[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02};
  uint64_t out = 0;
  size_t consumed = 0;
  assert(idl_uleb128_decode(buf, sizeof(buf), &consumed, &out) ==
         IDL_STATUS_ERR_OVERFLOW);
}

static void test_hash(void) {
  assert(idl_hash("name") == idl_hash("name"));
  assert(idl_hash("name") != idl_hash("value"));
  idl_field_id fields[] = {
      {.id = 5, .index = 1},
      {.id = 2, .index = 0},
      {.id = 30, .index = 2},
  };
  idl_field_id_sort(fields, 3);
  assert(fields[0].id == 2);
  assert(fields[1].id == 5);
  assert(fields[2].id == 30);
  assert(idl_field_id_verify_unique(fields, 3) == IDL_STATUS_OK);
  fields[1].id = 2;
  assert(idl_field_id_verify_unique(fields, 3) == IDL_STATUS_ERR_INVALID_ARG);
}

static void test_arena(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 128) == IDL_STATUS_OK);
  char *hello = (char *)idl_arena_alloc(&arena, 6);
  assert(hello);
  memcpy(hello, "hello", 6);
  char *copy = (char *)idl_arena_dup(&arena, "world", 6);
  assert(copy);
  assert(memcmp(copy, "world", 6) == 0);
  int *numbers = (int *)idl_arena_alloc_zeroed(&arena, 4 * sizeof(int));
  assert(numbers);
  for (int i = 0; i < 4; ++i) {
    assert(numbers[i] == 0);
  }
  idl_arena_destroy(&arena);
}

/* ========== Phase 2 Tests: Type System ========== */

static void test_type_primitives(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 1024) == IDL_STATUS_OK);

  /* Test primitive type creation */
  idl_type *t_null = idl_type_null(&arena);
  assert(t_null && t_null->kind == IDL_KIND_NULL);
  assert(idl_type_is_primitive(t_null));

  idl_type *t_bool = idl_type_bool(&arena);
  assert(t_bool && t_bool->kind == IDL_KIND_BOOL);
  assert(idl_type_is_primitive(t_bool));

  idl_type *t_nat64 = idl_type_nat64(&arena);
  assert(t_nat64 && t_nat64->kind == IDL_KIND_NAT64);
  assert(idl_type_is_primitive(t_nat64));

  idl_type *t_text = idl_type_text(&arena);
  assert(t_text && t_text->kind == IDL_KIND_TEXT);
  assert(idl_type_is_primitive(t_text));

  /* Test opcode mapping */
  assert(idl_type_opcode(IDL_KIND_NULL) == IDL_TYPE_NULL);
  assert(idl_type_opcode(IDL_KIND_NAT64) == IDL_TYPE_NAT64);
  assert(idl_type_opcode(IDL_KIND_TEXT) == IDL_TYPE_TEXT);

  idl_arena_destroy(&arena);
}

static void test_type_composite(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 1024) == IDL_STATUS_OK);

  /* Test opt type */
  idl_type *t_nat64 = idl_type_nat64(&arena);
  idl_type *t_opt = idl_type_opt(&arena, t_nat64);
  assert(t_opt && t_opt->kind == IDL_KIND_OPT);
  assert(!idl_type_is_primitive(t_opt));
  assert(t_opt->data.inner == t_nat64);

  /* Test vec type */
  idl_type *t_vec = idl_type_vec(&arena, t_nat64);
  assert(t_vec && t_vec->kind == IDL_KIND_VEC);
  assert(t_vec->data.inner == t_nat64);

  /* Test record type */
  idl_type *t_text = idl_type_text(&arena);
  idl_field fields[2];
  fields[0].label = idl_label_name("name");
  fields[0].type = t_text;
  fields[1].label = idl_label_name("age");
  fields[1].type = t_nat64;

  idl_type *t_record = idl_type_record(&arena, fields, 2);
  assert(t_record && t_record->kind == IDL_KIND_RECORD);
  assert(t_record->data.record.fields_len == 2);

  idl_arena_destroy(&arena);
}

static void test_type_env(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 1024) == IDL_STATUS_OK);

  idl_type_env env;
  assert(idl_type_env_init(&env, &arena) == IDL_STATUS_OK);

  idl_type *t_nat64 = idl_type_nat64(&arena);
  idl_type *t_text = idl_type_text(&arena);

  /* Insert types */
  assert(idl_type_env_insert(&env, "MyNat", t_nat64) == IDL_STATUS_OK);
  assert(idl_type_env_insert(&env, "MyText", t_text) == IDL_STATUS_OK);
  assert(idl_type_env_count(&env) == 2);

  /* Find types */
  assert(idl_type_env_find(&env, "MyNat") == t_nat64);
  assert(idl_type_env_find(&env, "MyText") == t_text);
  assert(idl_type_env_find(&env, "NotFound") == NULL);

  /* Duplicate insert with same type should succeed */
  assert(idl_type_env_insert(&env, "MyNat", t_nat64) == IDL_STATUS_OK);

  /* Duplicate insert with different type should fail */
  assert(idl_type_env_insert(&env, "MyNat", t_text) ==
         IDL_STATUS_ERR_INVALID_ARG);

  idl_arena_destroy(&arena);
}

static void test_type_table_builder(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  idl_type_table_builder builder;
  assert(idl_type_table_builder_init(&builder, &arena, NULL) == IDL_STATUS_OK);

  /* Push primitive types (should not add to type table) */
  idl_type *t_nat64 = idl_type_nat64(&arena);
  idl_type *t_text = idl_type_text(&arena);

  assert(idl_type_table_builder_push_arg(&builder, t_nat64) == IDL_STATUS_OK);
  assert(idl_type_table_builder_push_arg(&builder, t_text) == IDL_STATUS_OK);
  assert(builder.entries_count == 0); /* primitives don't go in table */
  assert(builder.args_count == 2);

  /* Push composite type */
  idl_type *t_opt = idl_type_opt(&arena, t_nat64);
  assert(idl_type_table_builder_push_arg(&builder, t_opt) == IDL_STATUS_OK);
  assert(builder.entries_count == 1); /* opt goes in table */
  assert(builder.args_count == 3);

  /* Serialize */
  uint8_t *data;
  size_t len;
  assert(idl_type_table_builder_serialize(&builder, &data, &len) ==
         IDL_STATUS_OK);
  assert(data != NULL);
  assert(len > 0);

  idl_arena_destroy(&arena);
}

static void test_header_parse(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  /*
   * Test parsing a simple DIDL message: ("hello", 42)
   * Hex: 4449444c0002717c0568656c6c6f2a
   * - DIDL magic: 44 49 44 4c
   * - Type table count: 00 (0 entries)
   * - Arg count: 02 (2 args)
   * - Arg 0 type: 71 (sleb128 -15 = text)
   * - Arg 1 type: 7c (sleb128 -4 = int)
   * - Values follow...
   */
  uint8_t data[] = {
      0x44, 0x49, 0x44, 0x4c, /* DIDL */
      0x00,                   /* type table count = 0 */
      0x02,                   /* arg count = 2 */
      0x71,                   /* text (-15) */
      0x7c,                   /* int (-4) */
      /* values would follow here but we only parse header */
  };

  idl_header header;
  size_t consumed;
  assert(idl_header_parse(data, sizeof(data), &arena, &header, &consumed) ==
         IDL_STATUS_OK);

  assert(header.arg_count == 2);
  assert(header.arg_types[0]->kind == IDL_KIND_TEXT);
  assert(header.arg_types[1]->kind == IDL_KIND_INT);
  assert(consumed == 8); /* 4 magic + 1 table_count + 1 arg_count + 2 args */

  idl_arena_destroy(&arena);
}

static void test_header_with_type_table(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  /*
   * Test parsing DIDL with type table: vec nat64
   * - DIDL magic
   * - Type table: 1 entry (vec with inner type nat64)
   * - Args: 1 arg referencing table[0]
   */
  uint8_t data[] = {
      0x44, 0x49, 0x44, 0x4c, /* DIDL */
      0x01,                   /* type table count = 1 */
      0x6d,                   /* vec opcode (-19) */
      0x78,                   /* inner type: nat64 (-8) */
      0x01,                   /* arg count = 1 */
      0x00,                   /* arg 0: table[0] */
  };

  idl_header header;
  size_t consumed;
  assert(idl_header_parse(data, sizeof(data), &arena, &header, &consumed) ==
         IDL_STATUS_OK);

  assert(header.arg_count == 1);
  /* Arg type is a VAR referencing table0 */
  assert(header.arg_types[0]->kind == IDL_KIND_VAR);

  /* Look up the actual type */
  idl_type *resolved = idl_type_env_find(&header.env, "table0");
  assert(resolved != NULL);
  assert(resolved->kind == IDL_KIND_VEC);
  assert(resolved->data.inner->kind == IDL_KIND_NAT64);

  idl_arena_destroy(&arena);
}

static void test_label(void) {
  /* Test label ID */
  idl_label l1 = idl_label_id(42);
  assert(l1.kind == IDL_LABEL_ID);
  assert(l1.id == 42);
  assert(l1.name == NULL);

  /* Test label name (hashed) */
  idl_label l2 = idl_label_name("name");
  assert(l2.kind == IDL_LABEL_NAME);
  assert(l2.id == idl_hash("name"));
  assert(strcmp(l2.name, "name") == 0);

  /* Different names should have different hashes (usually) */
  idl_label l3 = idl_label_name("age");
  assert(l3.id != l2.id);
}

/* ========== Phase 3 Tests: Value Serialization ========== */

static void test_value_primitives(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 1024) == IDL_STATUS_OK);

  /* Test value creation */
  idl_value *v_null = idl_value_null(&arena);
  assert(v_null && v_null->kind == IDL_VALUE_NULL);

  idl_value *v_bool = idl_value_bool(&arena, 1);
  assert(v_bool && v_bool->kind == IDL_VALUE_BOOL);
  assert(v_bool->data.bool_val == 1);

  idl_value *v_nat64 = idl_value_nat64(&arena, 42);
  assert(v_nat64 && v_nat64->kind == IDL_VALUE_NAT64);
  assert(v_nat64->data.nat64_val == 42);

  idl_value *v_int64 = idl_value_int64(&arena, -123);
  assert(v_int64 && v_int64->kind == IDL_VALUE_INT64);
  assert(v_int64->data.int64_val == -123);

  idl_value *v_text = idl_value_text_cstr(&arena, "hello");
  assert(v_text && v_text->kind == IDL_VALUE_TEXT);
  assert(v_text->data.text.len == 5);
  assert(strcmp(v_text->data.text.data, "hello") == 0);

  idl_arena_destroy(&arena);
}

static void test_value_serializer(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 1024) == IDL_STATUS_OK);

  idl_value_serializer ser;
  assert(idl_value_serializer_init(&ser, &arena) == IDL_STATUS_OK);

  /* Serialize a bool (true) */
  assert(idl_value_serializer_write_bool(&ser, 1) == IDL_STATUS_OK);
  assert(idl_value_serializer_len(&ser) == 1);
  assert(idl_value_serializer_data(&ser)[0] == 1);

  /* Serialize a nat64 (0x0102030405060708 in little-endian) */
  idl_value_serializer ser2;
  assert(idl_value_serializer_init(&ser2, &arena) == IDL_STATUS_OK);
  assert(idl_value_serializer_write_nat64(&ser2, 0x0807060504030201ULL) ==
         IDL_STATUS_OK);
  assert(idl_value_serializer_len(&ser2) == 8);
  const uint8_t *data = idl_value_serializer_data(&ser2);
  assert(data[0] == 0x01 && data[1] == 0x02 && data[2] == 0x03 &&
         data[3] == 0x04);
  assert(data[4] == 0x05 && data[5] == 0x06 && data[6] == 0x07 &&
         data[7] == 0x08);

  /* Serialize text */
  idl_value_serializer ser3;
  assert(idl_value_serializer_init(&ser3, &arena) == IDL_STATUS_OK);
  assert(idl_value_serializer_write_text(&ser3, "hi", 2) == IDL_STATUS_OK);
  assert(idl_value_serializer_len(&ser3) ==
         3); /* 1 byte LEB128 len + 2 bytes */
  data = idl_value_serializer_data(&ser3);
  assert(data[0] == 2); /* length */
  assert(data[1] == 'h' && data[2] == 'i');

  idl_arena_destroy(&arena);
}

static void test_builder_primitives(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  idl_builder builder;
  assert(idl_builder_init(&builder, &arena) == IDL_STATUS_OK);

  /* Build: (true) */
  assert(idl_builder_arg_bool(&builder, 1) == IDL_STATUS_OK);

  uint8_t *data;
  size_t len;
  assert(idl_builder_serialize(&builder, &data, &len) == IDL_STATUS_OK);

  /* Should start with DIDL magic */
  assert(len >= 4);
  assert(data[0] == 'D' && data[1] == 'I' && data[2] == 'D' && data[3] == 'L');

  idl_arena_destroy(&arena);
}

static void test_builder_text_int(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  idl_builder builder;
  assert(idl_builder_init(&builder, &arena) == IDL_STATUS_OK);

  /* Build: ("hello", 42 : int) - matches the oracle test case */
  assert(idl_builder_arg_text_cstr(&builder, "hello") == IDL_STATUS_OK);

  /* For int, we need to manually create the type and value */
  idl_type *t_int = idl_type_int(&arena);
  idl_value *v_int = idl_value_int_i64(&arena, 42);
  assert(t_int && v_int);
  assert(idl_builder_arg(&builder, t_int, v_int) == IDL_STATUS_OK);

  char *hex;
  size_t hex_len;
  assert(idl_builder_serialize_hex(&builder, &hex, &hex_len) == IDL_STATUS_OK);

  /*
   * Expected: 4449444c0002717c0568656c6c6f2a
   * - DIDL: 44 49 44 4c
   * - Type table count: 00
   * - Arg count: 02
   * - Arg 0 type: 71 (text = -15)
   * - Arg 1 type: 7c (int = -4)
   * - Value 0: 05 68 65 6c 6c 6f ("hello")
   * - Value 1: 2a (42 as SLEB128)
   */
  assert(strcmp(hex, "4449444c0002717c0568656c6c6f2a") == 0);

  idl_arena_destroy(&arena);
}

static void test_builder_vec(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  idl_builder builder;
  assert(idl_builder_init(&builder, &arena) == IDL_STATUS_OK);

  /* Build: vec { 1; 2; 3 } : vec nat8 */
  idl_type *t_nat8 = idl_type_nat8(&arena);
  idl_type *t_vec = idl_type_vec(&arena, t_nat8);

  idl_value *items[3];
  items[0] = idl_value_nat8(&arena, 1);
  items[1] = idl_value_nat8(&arena, 2);
  items[2] = idl_value_nat8(&arena, 3);
  idl_value *v_vec = idl_value_vec(&arena, items, 3);

  assert(idl_builder_arg(&builder, t_vec, v_vec) == IDL_STATUS_OK);

  uint8_t *data;
  size_t len;
  assert(idl_builder_serialize(&builder, &data, &len) == IDL_STATUS_OK);

  /* Verify DIDL magic */
  assert(data[0] == 'D' && data[1] == 'I' && data[2] == 'D' && data[3] == 'L');

  idl_arena_destroy(&arena);
}

static void test_builder_record(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  idl_builder builder;
  assert(idl_builder_init(&builder, &arena) == IDL_STATUS_OK);

  /* Build: record { name = "Alice"; age = 30 } */
  idl_type *t_text = idl_type_text(&arena);
  idl_type *t_nat32 = idl_type_nat32(&arena);

  /* Create type fields (must be sorted by hash) */
  idl_field type_fields[2];
  type_fields[0].label = idl_label_name("age");
  type_fields[0].type = t_nat32;
  type_fields[1].label = idl_label_name("name");
  type_fields[1].type = t_text;

  /* Sort fields by label id */
  if (type_fields[0].label.id > type_fields[1].label.id) {
    idl_field tmp = type_fields[0];
    type_fields[0] = type_fields[1];
    type_fields[1] = tmp;
  }

  idl_type *t_record = idl_type_record(&arena, type_fields, 2);

  /* Create value fields (in same order as type) */
  idl_value_field value_fields[2];
  if (type_fields[0].label.id == idl_hash("age")) {
    value_fields[0].label = idl_label_name("age");
    value_fields[0].value = idl_value_nat32(&arena, 30);
    value_fields[1].label = idl_label_name("name");
    value_fields[1].value = idl_value_text_cstr(&arena, "Alice");
  } else {
    value_fields[0].label = idl_label_name("name");
    value_fields[0].value = idl_value_text_cstr(&arena, "Alice");
    value_fields[1].label = idl_label_name("age");
    value_fields[1].value = idl_value_nat32(&arena, 30);
  }

  idl_value *v_record = idl_value_record(&arena, value_fields, 2);

  assert(idl_builder_arg(&builder, t_record, v_record) == IDL_STATUS_OK);

  uint8_t *data;
  size_t len;
  assert(idl_builder_serialize(&builder, &data, &len) == IDL_STATUS_OK);

  /* Verify DIDL magic */
  assert(data[0] == 'D' && data[1] == 'I' && data[2] == 'D' && data[3] == 'L');

  idl_arena_destroy(&arena);
}

/* ========== Phase 4 Tests: Value Deserialization ========== */

static void test_deserializer_primitives(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  /*
   * Test decoding a simple bool (true)
   * DIDL + 0 types + 1 arg (bool) + value (1)
   */
  uint8_t data[] = {
      0x44, 0x49, 0x44, 0x4c, /* DIDL */
      0x00,                   /* type table count = 0 */
      0x01,                   /* arg count = 1 */
      0x7e,                   /* bool (-2) */
      0x01,                   /* value: true */
  };

  idl_deserializer *de;
  assert(idl_deserializer_new(data, sizeof(data), &arena, &de) ==
         IDL_STATUS_OK);

  assert(!idl_deserializer_is_done(de));

  idl_type *type;
  idl_value *value;
  assert(idl_deserializer_get_value(de, &type, &value) == IDL_STATUS_OK);

  assert(value->kind == IDL_VALUE_BOOL);
  assert(value->data.bool_val == 1);

  assert(idl_deserializer_is_done(de));
  assert(idl_deserializer_done(de) == IDL_STATUS_OK);

  idl_arena_destroy(&arena);
}

static void test_deserializer_text_int(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  /*
   * Decode: ("hello", 42 : int)
   * Hex: 4449444c0002717c0568656c6c6f2a
   */
  uint8_t data[] = {0x44, 0x49, 0x44, 0x4c, 0x00, 0x02, 0x71, 0x7c,
                    0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2a};

  idl_deserializer *de;
  assert(idl_deserializer_new(data, sizeof(data), &arena, &de) ==
         IDL_STATUS_OK);

  /* First arg: text */
  idl_type *type1;
  idl_value *value1;
  assert(idl_deserializer_get_value(de, &type1, &value1) == IDL_STATUS_OK);
  assert(value1->kind == IDL_VALUE_TEXT);
  assert(value1->data.text.len == 5);
  assert(strcmp(value1->data.text.data, "hello") == 0);

  /* Second arg: int */
  idl_type *type2;
  idl_value *value2;
  assert(idl_deserializer_get_value(de, &type2, &value2) == IDL_STATUS_OK);
  assert(value2->kind == IDL_VALUE_INT);
  /* The int is stored as SLEB128 bytes; 42 = 0x2a */
  assert(value2->data.bignum.len == 1);
  assert(value2->data.bignum.data[0] == 0x2a);

  assert(idl_deserializer_is_done(de));
  assert(idl_deserializer_done(de) == IDL_STATUS_OK);

  idl_arena_destroy(&arena);
}

static void test_roundtrip_primitives(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 8192) == IDL_STATUS_OK);

  /* Encode */
  idl_builder builder;
  assert(idl_builder_init(&builder, &arena) == IDL_STATUS_OK);

  assert(idl_builder_arg_bool(&builder, 1) == IDL_STATUS_OK);
  assert(idl_builder_arg_nat64(&builder, 12345678901234ULL) == IDL_STATUS_OK);
  assert(idl_builder_arg_int32(&builder, -42) == IDL_STATUS_OK);
  assert(idl_builder_arg_text_cstr(&builder, "roundtrip") == IDL_STATUS_OK);

  uint8_t *data;
  size_t len;
  assert(idl_builder_serialize(&builder, &data, &len) == IDL_STATUS_OK);

  /* Decode */
  idl_deserializer *de;
  assert(idl_deserializer_new(data, len, &arena, &de) == IDL_STATUS_OK);

  idl_type *type;
  idl_value *value;

  /* bool */
  assert(idl_deserializer_get_value(de, &type, &value) == IDL_STATUS_OK);
  assert(value->kind == IDL_VALUE_BOOL);
  assert(value->data.bool_val == 1);

  /* nat64 */
  assert(idl_deserializer_get_value(de, &type, &value) == IDL_STATUS_OK);
  assert(value->kind == IDL_VALUE_NAT64);
  assert(value->data.nat64_val == 12345678901234ULL);

  /* int32 */
  assert(idl_deserializer_get_value(de, &type, &value) == IDL_STATUS_OK);
  assert(value->kind == IDL_VALUE_INT32);
  assert(value->data.int32_val == -42);

  /* text */
  assert(idl_deserializer_get_value(de, &type, &value) == IDL_STATUS_OK);
  assert(value->kind == IDL_VALUE_TEXT);
  assert(strcmp(value->data.text.data, "roundtrip") == 0);

  assert(idl_deserializer_is_done(de));
  assert(idl_deserializer_done(de) == IDL_STATUS_OK);

  idl_arena_destroy(&arena);
}

static void test_roundtrip_composite(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 8192) == IDL_STATUS_OK);

  /* Encode: vec { 10; 20; 30 } : vec nat8 */
  idl_builder builder;
  assert(idl_builder_init(&builder, &arena) == IDL_STATUS_OK);

  idl_type *t_nat8 = idl_type_nat8(&arena);
  idl_type *t_vec = idl_type_vec(&arena, t_nat8);

  idl_value *items[3];
  items[0] = idl_value_nat8(&arena, 10);
  items[1] = idl_value_nat8(&arena, 20);
  items[2] = idl_value_nat8(&arena, 30);
  idl_value *v_vec = idl_value_vec(&arena, items, 3);

  assert(idl_builder_arg(&builder, t_vec, v_vec) == IDL_STATUS_OK);

  uint8_t *data;
  size_t len;
  assert(idl_builder_serialize(&builder, &data, &len) == IDL_STATUS_OK);

  /* Decode */
  idl_deserializer *de;
  assert(idl_deserializer_new(data, len, &arena, &de) == IDL_STATUS_OK);

  idl_type *type;
  idl_value *value;
  assert(idl_deserializer_get_value(de, &type, &value) == IDL_STATUS_OK);

  /* vec nat8 is decoded as blob */
  assert(value->kind == IDL_VALUE_BLOB);
  assert(value->data.blob.len == 3);
  assert(value->data.blob.data[0] == 10);
  assert(value->data.blob.data[1] == 20);
  assert(value->data.blob.data[2] == 30);

  assert(idl_deserializer_is_done(de));
  assert(idl_deserializer_done(de) == IDL_STATUS_OK);

  idl_arena_destroy(&arena);
}

/* ========== Phase 5 Tests: Subtype and Coercion ========== */

static void test_subtype_primitives(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  idl_type *t_nat = idl_type_nat(&arena);
  idl_type *t_int = idl_type_int(&arena);
  idl_type *t_nat64 = idl_type_nat64(&arena);
  idl_type *t_text = idl_type_text(&arena);
  idl_type *t_reserved = idl_type_reserved(&arena);
  idl_type *t_empty = idl_type_empty(&arena);

  /* Same type is subtype */
  assert(idl_subtype(NULL, t_nat64, t_nat64, &arena) == IDL_SUBTYPE_OK);
  assert(idl_subtype(NULL, t_text, t_text, &arena) == IDL_SUBTYPE_OK);

  /* nat <: int */
  assert(idl_subtype(NULL, t_nat, t_int, &arena) == IDL_SUBTYPE_OK);

  /* int is NOT subtype of nat */
  assert(idl_subtype(NULL, t_int, t_nat, &arena) == IDL_SUBTYPE_FAIL);

  /* Everything <: reserved */
  assert(idl_subtype(NULL, t_nat64, t_reserved, &arena) == IDL_SUBTYPE_OK);
  assert(idl_subtype(NULL, t_text, t_reserved, &arena) == IDL_SUBTYPE_OK);

  /* empty <: everything */
  assert(idl_subtype(NULL, t_empty, t_nat64, &arena) == IDL_SUBTYPE_OK);
  assert(idl_subtype(NULL, t_empty, t_text, &arena) == IDL_SUBTYPE_OK);

  /* Different primitives are not subtypes */
  assert(idl_subtype(NULL, t_nat64, t_text, &arena) == IDL_SUBTYPE_FAIL);

  idl_arena_destroy(&arena);
}

static void test_subtype_opt(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  idl_type *t_nat64 = idl_type_nat64(&arena);
  idl_type *t_null = idl_type_null(&arena);
  idl_type *t_opt_nat64 = idl_type_opt(&arena, t_nat64);

  /* null <: opt T */
  assert(idl_subtype(NULL, t_null, t_opt_nat64, &arena) == IDL_SUBTYPE_OK);

  /* opt T <: opt T */
  assert(idl_subtype(NULL, t_opt_nat64, t_opt_nat64, &arena) == IDL_SUBTYPE_OK);

  /* T <: opt T (if T is not null/reserved/opt) */
  assert(idl_subtype(NULL, t_nat64, t_opt_nat64, &arena) == IDL_SUBTYPE_OK);

  idl_arena_destroy(&arena);
}

static void test_subtype_record(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  idl_type *t_nat64 = idl_type_nat64(&arena);
  idl_type *t_text = idl_type_text(&arena);
  idl_type *t_opt_text = idl_type_opt(&arena, t_text);

  /* record { a: nat64 } */
  idl_field fields1[1];
  fields1[0].label = idl_label_name("a");
  fields1[0].type = t_nat64;
  idl_type *t_rec1 = idl_type_record(&arena, fields1, 1);

  /* record { a: nat64; b: text } - superset */
  idl_field fields2[2];
  fields2[0].label = idl_label_name("a");
  fields2[0].type = t_nat64;
  fields2[1].label = idl_label_name("b");
  fields2[1].type = t_text;
  idl_type *t_rec2 = idl_type_record(&arena, fields2, 2);

  /* record { a: nat64; c: opt text } - different optional field */
  idl_field fields3[2];
  fields3[0].label = idl_label_name("a");
  fields3[0].type = t_nat64;
  fields3[1].label = idl_label_name("c");
  fields3[1].type = t_opt_text;
  idl_type *t_rec3 = idl_type_record(&arena, fields3, 2);

  /* Superset is subtype (extra fields OK) */
  assert(idl_subtype(NULL, t_rec2, t_rec1, &arena) == IDL_SUBTYPE_OK);

  /* Subset is NOT subtype (missing required field) */
  assert(idl_subtype(NULL, t_rec1, t_rec2, &arena) == IDL_SUBTYPE_FAIL);

  /* Missing optional field is OK */
  assert(idl_subtype(NULL, t_rec1, t_rec3, &arena) == IDL_SUBTYPE_OK);

  idl_arena_destroy(&arena);
}

static void test_coerce_opt(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  idl_type *t_nat64 = idl_type_nat64(&arena);
  idl_type *t_null = idl_type_null(&arena);
  idl_type *t_opt_nat64 = idl_type_opt(&arena, t_nat64);

  /* Coerce null to opt nat64 -> None */
  idl_value *v_null = idl_value_null(&arena);
  idl_value *coerced;
  assert(idl_coerce_value(&arena, NULL, t_null, t_opt_nat64, v_null,
                          &coerced) == IDL_STATUS_OK);
  assert(coerced->kind == IDL_VALUE_OPT);
  assert(coerced->data.opt == NULL);

  /* Coerce nat64 to opt nat64 -> Some(nat64) */
  idl_value *v_nat64 = idl_value_nat64(&arena, 42);
  assert(idl_coerce_value(&arena, NULL, t_nat64, t_opt_nat64, v_nat64,
                          &coerced) == IDL_STATUS_OK);
  assert(coerced->kind == IDL_VALUE_OPT);
  assert(coerced->data.opt != NULL);
  assert(coerced->data.opt->kind == IDL_VALUE_NAT64);
  assert(coerced->data.opt->data.nat64_val == 42);

  idl_arena_destroy(&arena);
}

static void test_coerce_record(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  idl_type *t_nat64 = idl_type_nat64(&arena);
  idl_type *t_text = idl_type_text(&arena);
  idl_type *t_opt_text = idl_type_opt(&arena, t_text);

  /* Wire type: record { a: nat64; b: text } */
  idl_field wire_fields[2];
  wire_fields[0].label = idl_label_name("a");
  wire_fields[0].type = t_nat64;
  wire_fields[1].label = idl_label_name("b");
  wire_fields[1].type = t_text;
  idl_type *t_wire = idl_type_record(&arena, wire_fields, 2);

  /* Expected type: record { a: nat64 } (subset) */
  idl_field exp_fields[1];
  exp_fields[0].label = idl_label_name("a");
  exp_fields[0].type = t_nat64;
  idl_type *t_expected = idl_type_record(&arena, exp_fields, 1);

  /* Wire value: { a = 42; b = "hello" } */
  idl_value_field wire_val_fields[2];
  wire_val_fields[0].label = idl_label_name("a");
  wire_val_fields[0].value = idl_value_nat64(&arena, 42);
  wire_val_fields[1].label = idl_label_name("b");
  wire_val_fields[1].value = idl_value_text_cstr(&arena, "hello");
  idl_value *v_wire = idl_value_record(&arena, wire_val_fields, 2);

  /* Coerce to expected type (should drop field b) */
  idl_value *coerced;
  assert(idl_coerce_value(&arena, NULL, t_wire, t_expected, v_wire, &coerced) ==
         IDL_STATUS_OK);
  assert(coerced->kind == IDL_VALUE_RECORD);
  assert(coerced->data.record.len == 1);
  assert(coerced->data.record.fields[0].label.id == idl_hash("a"));
  assert(coerced->data.record.fields[0].value->kind == IDL_VALUE_NAT64);
  assert(coerced->data.record.fields[0].value->data.nat64_val == 42);

  /* Expected type with optional field: record { a: nat64; c: opt text } */
  idl_field exp_fields2[2];
  exp_fields2[0].label = idl_label_name("a");
  exp_fields2[0].type = t_nat64;
  exp_fields2[1].label = idl_label_name("c");
  exp_fields2[1].type = t_opt_text;
  idl_type *t_expected2 = idl_type_record(&arena, exp_fields2, 2);

  /* Coerce to expected type (should fill missing c with None) */
  assert(idl_coerce_value(&arena, NULL, t_wire, t_expected2, v_wire,
                          &coerced) == IDL_STATUS_OK);
  assert(coerced->kind == IDL_VALUE_RECORD);
  assert(coerced->data.record.len == 2);

  idl_arena_destroy(&arena);
}

static void test_decoder_quota(void) {
  idl_arena arena;
  assert(idl_arena_init(&arena, 4096) == IDL_STATUS_OK);

  /* Simple DIDL message: (true) */
  uint8_t data[] = {
      0x44, 0x49, 0x44, 0x4c, /* DIDL */
      0x00,                   /* type table count = 0 */
      0x01,                   /* arg count = 1 */
      0x7e,                   /* bool (-2) */
      0x01,                   /* value: true */
  };

  /* Test with sufficient quota */
  idl_decoder_config config;
  idl_decoder_config_init(&config);
  idl_decoder_config_set_decoding_quota(&config, 1000);

  idl_deserializer *de;
  assert(idl_deserializer_new_with_config(data, sizeof(data), &arena, &config,
                                          &de) == IDL_STATUS_OK);

  idl_type *type;
  idl_value *value;
  assert(idl_deserializer_get_value(de, &type, &value) == IDL_STATUS_OK);
  assert(value->kind == IDL_VALUE_BOOL);

  /* Test with very small quota (should fail on header parsing) */
  idl_decoder_config config2;
  idl_decoder_config_init(&config2);
  idl_decoder_config_set_decoding_quota(&config2, 1); /* too small */

  idl_deserializer *de2;
  /* Header parsing costs 8 * 4 = 32, so quota of 1 should fail */
  assert(idl_deserializer_new_with_config(data, sizeof(data), &arena, &config2,
                                          &de2) == IDL_STATUS_ERR_OVERFLOW);

  idl_arena_destroy(&arena);
}

int main(void) {
  test_uleb128_roundtrip(0);
  test_uleb128_roundtrip(1);
  test_uleb128_roundtrip(127);
  test_uleb128_roundtrip(128);
  test_uleb128_roundtrip(UINT64_C(0xffffffffffffffff));

  test_sleb128_roundtrip(0);
  test_sleb128_roundtrip(-1);
  test_sleb128_roundtrip(63);
  test_sleb128_roundtrip(-64);
  test_sleb128_roundtrip(INT64_C(0x7fffffffffffffff));
  test_sleb128_roundtrip(INT64_C(-0x7fffffffffffffff - 1));

  test_uleb128_overflow();
  test_hash();
  test_arena();

  /* Phase 2 tests */
  test_type_primitives();
  test_type_composite();
  test_type_env();
  test_type_table_builder();
  test_header_parse();
  test_header_with_type_table();
  test_label();

  /* Phase 3 tests */
  test_value_primitives();
  test_value_serializer();
  test_builder_primitives();
  test_builder_text_int();
  test_builder_vec();
  test_builder_record();

  /* Phase 4 tests */
  test_deserializer_primitives();
  test_deserializer_text_int();
  test_roundtrip_primitives();
  test_roundtrip_composite();

  /* Phase 5 tests */
  test_subtype_primitives();
  test_subtype_opt();
  test_subtype_record();
  test_coerce_opt();
  test_coerce_record();
  test_decoder_quota();

  puts("runtime tests passed");
  return 0;
}
