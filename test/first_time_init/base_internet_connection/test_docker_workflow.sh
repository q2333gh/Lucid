#!/bin/bash
set -e

# Test script for the workflow described in ic-forum-release-post-draft.md
# This script tests the complete workflow:
# 1. git clone
# 2. cd Lucid
# 3. python build.py --new my_canister
# 4. python build.py --icwasm --examples my_canister
# 5. pytest examples/my_canister/test/

# Detect if docker command needs sudo
DOCKER_CMD="docker"
if ! docker info >/dev/null 2>&1; then
    if sudo docker info >/dev/null 2>&1; then
        DOCKER_CMD="sudo docker"
        echo "Note: Using sudo for docker commands"
    else
        echo "Error: Cannot run docker commands (with or without sudo)"
        exit 1
    fi
fi

echo "=========================================="
echo "Testing Lucid workflow in Docker"
echo "=========================================="

# Ensure we run docker build from this script's directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Build Docker image
echo ""
echo "Step 1: Building Docker image..."
echo "Using Docker command: ${DOCKER_CMD}"
$DOCKER_CMD build -f Dockerfile.test -t lucid-test:latest .

# Create a test script that will run inside the container
# Use a user-writable temp directory
TEMP_DIR=$(mktemp -d)
TEST_SCRIPT="$TEMP_DIR/test_workflow.sh"

cat > "$TEST_SCRIPT" << 'EOF'
#!/bin/bash
set -e

# Detect if running as root or need sudo for certain operations
SUDO=""
if [ "$(id -u)" != "0" ]; then
    # Not root, check if sudo is available and needed
    if command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    fi
fi

echo ""
echo "Step 2: Cloning repository..."
cd /tmp
rm -rf Lucid
git clone https://github.com/q2333gh/Lucid --depth=1
cd Lucid

echo ""
echo "Step 3: Installing Python dependencies..."
# Use --user flag to avoid needing sudo, or use sudo if needed
if [ "$(id -u)" = "0" ]; then
    python3.11 -m pip install -r requirements.txt
else
    python3.11 -m pip install --user -r requirements.txt || \
    $SUDO python3.11 -m pip install -r requirements.txt
fi

echo ""
echo "Step 4: Creating new project 'my_canister'..."
python3.11 build.py --new my_canister

echo ""
echo "Step 5: Creating test directory and test file..."
mkdir -p examples/my_canister/test

# Create a simple pytest test file
cat > examples/my_canister/test/test_my_canister.py << 'PYEOF'
#!/usr/bin/env python3
"""
Test suite for my_canister using pytest and PocketIC
"""
import sys
from pathlib import Path

# Add project root to path
project_root = Path(__file__).resolve().parent.parent.parent.parent
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

import pytest
from pocket_ic import PocketIC
from ic.candid import encode
from test.support.test_support_pocketic import install_example_canister, decode_candid_text


def test_greet():
    """Test the greet query method"""
    # Since we already built the canister, use auto_build=False
    pic, canister_id = install_example_canister("my_canister", auto_build=False)
    
    # Call greet method (empty args)
    payload = encode([])
    response_bytes = pic.query_call(canister_id, "greet", payload)
    
    # Decode response
    result = decode_candid_text(response_bytes)
    
    # Verify response contains expected text
    # The response might be in the decoded text or in the raw bytes
    expected = "Hello from minimal C canister!"
    assert expected in result or expected.encode('utf-8') in response_bytes, \
        f"Expected '{expected}' not found in response. Got: {result}"
PYEOF

chmod +x examples/my_canister/test/test_my_canister.py

echo ""
echo "Step 6: Building canister..."
export WASI_SDK_ROOT=/opt/wasi-sdk/wasi-sdk-25.0-x86_64-linux
python3.11 build.py --icwasm --examples my_canister

echo ""
echo "Step 7: Running tests with pytest..."
cd /tmp/Lucid
pytest examples/my_canister/test/ -v

echo ""
echo "=========================================="
echo "✓ All tests passed!"
echo "=========================================="
EOF

chmod +x "$TEST_SCRIPT"

# Run the test script inside the container
echo ""
echo "Step 2-7: Running workflow inside Docker container..."
$DOCKER_CMD run --rm \
    -v "$TEST_SCRIPT:/test_workflow.sh:ro" \
    lucid-test:latest \
    /bin/bash /test_workflow.sh

# Cleanup
rm -rf "$TEMP_DIR"

echo ""
echo "=========================================="
echo "✓ Docker workflow test completed successfully!"
echo "=========================================="
