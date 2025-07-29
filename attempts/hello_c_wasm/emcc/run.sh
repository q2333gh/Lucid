#!/bin/bash

# Set up Emscripten environment
export PATH="$(pwd)/emsdk:$(pwd)/emsdk/upstream/emscripten:$PATH"

# Check if WASM files exist, if not compile them
if [ ! -f "add.js" ] || [ ! -f "add.wasm" ]; then
    echo "Compiling C to WebAssembly..."
    make
fi

# Get WSL IP address
WSL_IP=$(ip route get 1.1.1.1 | awk '{print $7}' | head -1)
if [ -z "$WSL_IP" ]; then
    WSL_IP=$(hostname -I | awk '{print $1}')
fi

echo "Starting HTTP server..."
echo "WSL IP Address: $WSL_IP"
echo "Open your browser and navigate to:"
echo "  http://$WSL_IP:8000"
echo "  or"
echo "  http://localhost:8000 (if accessing from Windows host)"
echo ""
echo "Press Ctrl+C to stop the server"

# Start a simple HTTP server to serve the files
python3 -m http.server 8000