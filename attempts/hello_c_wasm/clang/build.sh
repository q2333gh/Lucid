#!/bin/bash
# 
# Modern WASM Build Script Wrapper
# Uses uv to manage Python dependencies and run the build script
#

# Check if uv is installed
if ! command -v uv &> /dev/null; then
    echo "❌ uv is not installed. Please install it first:"
    echo "   curl -LsSf https://astral.sh/uv/install.sh | sh"
    exit 1
fi

# Run the build script with uv, passing all arguments
exec uv run python build.py "$@"