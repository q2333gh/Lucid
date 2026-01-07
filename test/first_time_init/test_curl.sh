#!/bin/bash
set -e

# Minimal wget test to Google

DOCKER_CMD="docker"
if ! docker info >/dev/null 2>&1; then
    if sudo docker info >/dev/null 2>&1; then
        DOCKER_CMD="sudo docker"
    else
        echo "Error: Cannot run docker commands (with or without sudo)"
        exit 1
    fi
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Building minimal wget test image..."
$DOCKER_CMD build -f Dockerfile.curl-test -t wget-test:latest . >/dev/null

echo "Running wget https://www.google.com ..."
$DOCKER_CMD run --rm wget-test:latest wget -qO- https://www.google.com >/dev/null \
  && echo "wget ok" \
  || { echo "wget failed"; exit 1; }
