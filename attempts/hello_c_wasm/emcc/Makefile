CC = emcc
CFLAGS = -O2 -s WASM=1 -s EXPORTED_FUNCTIONS='["_add_exported"]' -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'

# Target WASM file
TARGET = add.wasm
JS_TARGET = add.js

# Source files
SOURCES = add.c

# Default target
all: $(TARGET)

# Compile C to WASM
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(JS_TARGET)

# Clean build files
clean:
	rm -f $(TARGET) $(JS_TARGET)

# Install Emscripten (if not already installed)
install-emscripten:
	git clone https://github.com/emscripten-core/emsdk.git
	cd emsdk && ./emsdk install latest && ./emsdk activate latest

.PHONY: all clean install-emscripten