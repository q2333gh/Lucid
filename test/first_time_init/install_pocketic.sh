#!/bin/bash
set -e

POCKETIC_DIR="/opt/pocketIC"
POCKETIC_BIN="$POCKETIC_DIR/pocket-ic"

sudo mkdir -p "$POCKETIC_DIR"
LATEST_VERSION=$(curl -s https://api.github.com/repos/dfinity/pocketic/releases/latest | grep -o '"tag_name": "[^"]*"' | head -1 | cut -d'"' -f4)
URL="https://github.com/dfinity/pocketic/releases/download/${LATEST_VERSION}/pocket-ic-x86_64-linux.gz"

curl -L "$URL" | gunzip -c | sudo tee "$POCKETIC_BIN" > /dev/null
sudo chmod +x "$POCKETIC_BIN"
echo "POCKET_IC_BIN set to $POCKETIC_BIN"

# Persist POCKET_IC_BIN environment variable
if grep -q 'export POCKET_IC_BIN=' ~/.bashrc; then
    sed -i 's|export POCKET_IC_BIN=.*|export POCKET_IC_BIN="/opt/pocketIC/pocket-ic"|' ~/.bashrc
else
    echo 'export POCKET_IC_BIN="/opt/pocketIC/pocket-ic"' >> ~/.bashrc
fi

source ~/.bashrc
