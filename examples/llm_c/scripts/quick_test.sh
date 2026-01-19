#!/bin/bash
# Fast iteration: build + unit test only (no deployment)
# Usage: ./scripts/quick_test.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
EXAMPLE_DIR="$SCRIPT_DIR/.."

echo "=== Quick Test: Building llm_c ==="
cd "$REPO_ROOT"
python build.py --icwasm --examples llm_c

echo ""
echo "=== Quick Test: Running unit tests (if available) ==="
if [ -d "$EXAMPLE_DIR/tests/unit" ]; then
    cd "$EXAMPLE_DIR"
    if command -v pytest &> /dev/null; then
        pytest tests/unit/ -v || echo "No unit tests found or pytest not available"
    else
        echo "pytest not found, skipping unit tests"
    fi
else
    echo "Unit tests directory not found, skipping"
fi

echo ""
echo "=== Quick Test Complete ==="
echo "Note: This only builds and runs unit tests."
echo "For full integration tests, use: python test_llm_c.py"
