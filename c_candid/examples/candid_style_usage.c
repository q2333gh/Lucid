/**
 * candid_style_usage.c - Example using CandidType* style naming
 *
 * This example shows the alternative API with CandidType*, CandidValue*
 * naming conventions that may be more familiar to users.
 *
 * Compare with basic_usage.c which uses the idl_* naming.
 */

#include "idl/candid.h" /* Use candid.h instead of runtime.h */
#include <stdio.h>

static void print_hex(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    printf("%02x", data[i]);
  }
  printf("\n");
}

/* Example 1: Simple primitives with CandidType* style */
static void example_primitives(void) {
  printf("=== Example 1: Primitives (CandidType* style) ===\n");

  CandidArena arena;
  CandidArenaInit(&arena, 4096);

  CandidBuilder builder;
  CandidBuilderInit(&builder, &arena);

  /* Add arguments using Candid* macros */
  CandidBuilderArgBool(&builder, 1);
  CandidBuilderArgNat64(&builder, 42);
  CandidBuilderArgText(&builder, "hello", 5);

  uint8_t *bytes;
  size_t len;
  if (CandidBuilderSerialize(&builder, &bytes, &len) == CANDID_OK) {
    printf("Encoded: ");
    print_hex(bytes, len);
  }

  CandidArenaDestroy(&arena);
  printf("\n");
}

/* Example 2: Building types with CandidType* constructors */
static void example_types(void) {
  printf("=== Example 2: Type constructors ===\n");

  CandidArena arena;
  CandidArenaInit(&arena, 4096);

  /* Create various types using CandidType* macros */
  CandidType *t_text = CandidTypeText(&arena);
  CandidType *t_nat32 = CandidTypeNat32(&arena);
  CandidType *t_principal = CandidTypePrincipal(&arena);
  CandidType *t_bool = CandidTypeBool(&arena);

  /* Create optional and vector types */
  CandidType *t_opt_text = CandidTypeOpt(&arena, CandidTypeText(&arena));
  CandidType *t_vec_nat8 = CandidTypeVec(&arena, CandidTypeNat8(&arena));

  printf("Created types:\n");
  printf("  text:      kind=%d\n", t_text->kind);
  printf("  nat32:     kind=%d\n", t_nat32->kind);
  printf("  principal: kind=%d\n", t_principal->kind);
  printf("  bool:      kind=%d\n", t_bool->kind);
  printf("  opt text:  kind=%d\n", t_opt_text->kind);
  printf("  vec nat8:  kind=%d\n", t_vec_nat8->kind);

  CandidArenaDestroy(&arena);
  printf("\n");
}

/* Example 3: Record with CandidType* style */
static void example_record(void) {
  printf("=== Example 3: Record ===\n");

  CandidArena arena;
  CandidArenaInit(&arena, 4096);

  CandidBuilder builder;
  CandidBuilderInit(&builder, &arena);

  /* Define record type: { name: text; age: nat16; active: bool } */
  CandidField fields[3];

  fields[0].label.kind = IDL_LABEL_NAME;
  fields[0].label.id = CandidHash("name");
  fields[0].label.name = "name";
  fields[0].type = CandidTypeText(&arena);

  fields[1].label.kind = IDL_LABEL_NAME;
  fields[1].label.id = CandidHash("age");
  fields[1].label.name = "age";
  fields[1].type = CandidTypeNat16(&arena);

  fields[2].label.kind = IDL_LABEL_NAME;
  fields[2].label.id = CandidHash("active");
  fields[2].label.name = "active";
  fields[2].type = CandidTypeBool(&arena);

  CandidType *record_type = CandidTypeRecord(&arena, fields, 3);

  /* Create record value */
  idl_value_field value_fields[3];

  value_fields[0].label = fields[0].label;
  value_fields[0].value = CandidValueText(&arena, "Bob", 3);

  value_fields[1].label = fields[1].label;
  value_fields[1].value = CandidValueNat16(&arena, 25);

  value_fields[2].label = fields[2].label;
  value_fields[2].value = CandidValueBool(&arena, 1);

  CandidValue *record_value = CandidValueRecord(&arena, value_fields, 3);

  /* Serialize */
  CandidBuilderArg(&builder, record_type, record_value);

  char *hex;
  size_t hex_len;
  if (CandidBuilderSerializeHex(&builder, &hex, &hex_len) == CANDID_OK) {
    printf("Record: %s\n", hex);
  }

  CandidArenaDestroy(&arena);
  printf("\n");
}

/* Example 4: Decode with CandidDeserializer */
static void example_decode(void) {
  printf("=== Example 4: Decode ===\n");

  /* Pre-encoded: ("hello", 42) */
  const uint8_t encoded[] = {0x44, 0x49, 0x44, 0x4c, 0x00, 0x02, 0x71, 0x7c,
                             0x05, 'h',  'e',  'l',  'l',  'o',  0x2a};

  CandidArena arena;
  CandidArenaInit(&arena, 4096);

  CandidDeserializer *de = NULL;
  if (CandidDeserializerNew(encoded, sizeof(encoded), &arena, &de) !=
      CANDID_OK) {
    printf("Failed to parse\n");
    CandidArenaDestroy(&arena);
    return;
  }

  printf("Decoded values:\n");
  while (!CandidDeserializerIsDone(de)) {
    CandidType *type;
    CandidValue *value;
    CandidDeserializerGetValue(de, &type, &value);

    switch (value->kind) {
    case CANDID_VALUE_TEXT:
      printf("  text: \"%.*s\"\n", (int)value->data.text.len,
             value->data.text.data);
      break;
    case CANDID_VALUE_INT:
      printf("  int: (bignum)\n");
      break;
    case CANDID_VALUE_NAT64:
      printf("  nat64: %lu\n", (unsigned long)value->data.nat64_val);
      break;
    default:
      printf("  kind=%d\n", value->kind);
      break;
    }
  }

  CandidArenaDestroy(&arena);
  printf("\n");
}

int main(void) {
  example_primitives();
  example_types();
  example_record();
  example_decode();

  printf("All CandidType* style examples completed!\n");
  return 0;
}
