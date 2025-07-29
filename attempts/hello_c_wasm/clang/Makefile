# Use WASI SDK clang for pure function
WASI_SDK_PATH = $(shell pwd)/wasi-sdk-21.0
CC = $(WASI_SDK_PATH)/bin/clang
CFLAGS = --target=wasm32-wasi
LDFLAGS = -Wl,--export-all -Wl,--no-entry

# Target WASM file
TARGET = add.wasm

# Source files
SOURCES = add.c

# Default target
all: $(TARGET)

# Compile C to WASM using Clang with WASI target
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES) -o $(TARGET)

# Clean build files
clean:
	rm -f $(TARGET)

# Test the WASM function using wasmtime --invoke
test: $(TARGET)
	@echo "Testing pure WASI add function with wasmtime --invoke..."
	@echo "Running: wasmtime --invoke add $(TARGET) 5 3"
	@wasmtime --invoke add $(TARGET) 5 3
	@echo ""
	@echo "Running: wasmtime --invoke add $(TARGET) 10 20"
	@wasmtime --invoke add $(TARGET) 10 20
	@echo ""
	@echo "Running: wasmtime --invoke add $(TARGET) 100 200"
	@wasmtime --invoke add $(TARGET) 100 200

# Show WASM file info and exports
info: $(TARGET)
	@echo "WASM file information:"
	@file $(TARGET)
	@echo ""
	@echo "WASM exports:"
	@wasmtime --wasm-features all $(TARGET) --invoke add 5 3

# Show WAT (WebAssembly Text) format
wat: $(TARGET)
	@echo "WebAssembly Text (WAT) format:"
	@echo "=============================="
	@wasm2wat $(TARGET)

.PHONY: all clean test info wat