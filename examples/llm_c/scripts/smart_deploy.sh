#!/bin/bash
# Smart deploy: only rebuild/redeploy if code changed
# Usage: ./scripts/smart_deploy.sh [--force]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
EXAMPLE_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$REPO_ROOT/build-wasi/examples/llm_c"
WASM_FILE="$BUILD_DIR/llm_c.wasm"
LAST_BUILD_FILE="$EXAMPLE_DIR/.last_build_hash"

FORCE=false
if [ "$1" == "--force" ]; then
    FORCE=true
fi

# Calculate hash of source files
calculate_source_hash() {
    find "$EXAMPLE_DIR" -name "*.c" -o -name "*.h" | sort | xargs sha256sum | sha256sum | cut -d' ' -f1
}

# Check if rebuild is needed
needs_rebuild() {
    if [ "$FORCE" == "true" ]; then
        return 0
    fi
    
    if [ ! -f "$WASM_FILE" ]; then
        return 0
    fi
    
    if [ ! -f "$LAST_BUILD_FILE" ]; then
        return 0
    fi
    
    CURRENT_HASH=$(calculate_source_hash)
    LAST_HASH=$(cat "$LAST_BUILD_FILE" 2>/dev/null || echo "")
    
    if [ "$CURRENT_HASH" != "$LAST_HASH" ]; then
        return 0
    fi
    
    return 1
}

echo "=== Smart Deploy: Checking if rebuild needed ==="

if needs_rebuild; then
    echo "Source code changed or build missing, rebuilding..."
    cd "$REPO_ROOT"
    python build.py --icwasm --examples llm_c
    
    # Save hash
    calculate_source_hash > "$LAST_BUILD_FILE"
    echo "Build hash saved"
    
    echo ""
    echo "=== Deploying canister ==="
    cd "$EXAMPLE_DIR"
    unset HTTP_PROXY http_proxy HTTPS_PROXY https_proxy ALL_PROXY all_proxy
    dfx deploy llm_c --no-wallet
else
    echo "No source changes detected, skipping build"
    echo "Use --force to force rebuild and deploy"
fi

echo ""
echo "=== Smart Deploy Complete ==="
