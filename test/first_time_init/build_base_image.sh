#!/bin/bash
set -e

# Build pre-updated Ubuntu 22.04 base image
# This image has apt-get update already executed for faster builds

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Detect if docker command needs sudo
DOCKER_CMD="docker"
if ! docker info >/dev/null 2>&1; then
    if sudo docker info >/dev/null 2>&1; then
        DOCKER_CMD="sudo docker"
    else
        echo "Error: Cannot run docker commands"
        exit 1
    fi
fi

echo "Building pre-updated Ubuntu 22.04 base image..."
$DOCKER_CMD build -f Dockerfile.base -t ubuntu:22.04-updated .

echo "✓ Base image built: ubuntu:22.04-updated"
echo "You can now use this image as base for faster builds"
