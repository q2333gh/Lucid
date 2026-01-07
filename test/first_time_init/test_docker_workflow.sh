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

# Build base image if not exists
echo ""
echo "Step 0: Ensuring base image exists..."
if ! $DOCKER_CMD images -q ubuntu:22.04-updated >/dev/null 2>&1; then
    echo "Building base image..."
    $DOCKER_CMD build -f Dockerfile.base -t ubuntu:22.04-updated .
else
    echo "Base image already exists, skipping..."
fi

# Build Docker image
echo ""
echo "Step 1: Building Docker image..."
echo "Using Docker command: ${DOCKER_CMD}"
$DOCKER_CMD build -f Dockerfile.test -t lucid-test:latest .

# Run the test script inside the container
echo ""
echo "Step 2-7: Running workflow inside Docker container..."
$DOCKER_CMD run --rm \
    lucid-test:latest \
    /usr/local/bin/test_workflow_inside_container.sh

echo ""
echo "=========================================="
echo "✓ Docker workflow test completed successfully!"
echo "=========================================="
