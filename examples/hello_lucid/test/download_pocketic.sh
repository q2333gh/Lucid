#!/bin/bash
# Download PocketIC server binary for Linux x86_64

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Get latest version from GitHub API
LATEST_VERSION=$(curl -s https://api.github.com/repos/dfinity/pocketic/releases/latest | grep -o '"tag_name": "[^"]*"' | head -1 | cut -d'"' -f4)

if [ -z "$LATEST_VERSION" ]; then
    echo "Error: Could not determine latest version"
    exit 1
fi

echo "Downloading PocketIC server version $LATEST_VERSION..."

# Download the binary (it's a .gz compressed file)
DOWNLOAD_URL="https://github.com/dfinity/pocketic/releases/download/${LATEST_VERSION}/pocket-ic-x86_64-linux.gz"
TEMP_FILE="pocket-ic-x86_64-linux.gz"

echo "Downloading from: $DOWNLOAD_URL"
curl -L -o "$TEMP_FILE" "$DOWNLOAD_URL"

# Check if download was successful
if [ ! -s "$TEMP_FILE" ]; then
    echo "Error: Download failed or file is empty"
    rm -f "$TEMP_FILE"
    exit 1
fi

# Extract the gz file
echo "Extracting..."
gunzip -f "$TEMP_FILE"

# Rename to pocket-ic if needed
if [ -f "pocket-ic-x86_64-linux" ]; then
    mv "pocket-ic-x86_64-linux" "pocket-ic"
fi

# Make it executable
chmod +x pocket-ic

echo "✓ PocketIC server downloaded successfully to: $SCRIPT_DIR/pocket-ic"
echo "You can now run: python3 t1.py"

