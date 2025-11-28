CC ?= cc
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
CFLAGS += -Iruntime/include
BUILD_DIR ?= build
BIN_DIR := $(BUILD_DIR)/bin
RUNTIME_OBJ_DIR := $(BUILD_DIR)/runtime

RUNTIME_SRCS := $(wildcard runtime/src/*.c)
RUNTIME_OBJS := $(patsubst runtime/src/%.c,$(RUNTIME_OBJ_DIR)/%.o,$(RUNTIME_SRCS))
LIB_RUNTIME := $(BUILD_DIR)/libcandid_runtime.a

SRC_DIR := src
ENCODE_BIN := $(BIN_DIR)/c_candid_encode
DECODE_BIN := $(BIN_DIR)/c_candid_decode
TESTS_BIN := $(BIN_DIR)/runtime_tests

.PHONY: all runtime bins tests clean

all: $(LIB_RUNTIME)

runtime: $(LIB_RUNTIME)

bins: $(ENCODE_BIN) $(DECODE_BIN)

tests: $(TESTS_BIN)
	$(TESTS_BIN)

$(RUNTIME_OBJ_DIR)/%.o: runtime/src/%.c
	@mkdir -p $(RUNTIME_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_RUNTIME): $(RUNTIME_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(AR) rcs $@ $^

$(BIN_DIR)/%: $(SRC_DIR)/%.c $(LIB_RUNTIME)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lcandid_runtime -o $@

$(TESTS_BIN): tests/runtime_tests.c $(LIB_RUNTIME)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lcandid_runtime -o $@

clean:
	rm -rf $(BUILD_DIR)


