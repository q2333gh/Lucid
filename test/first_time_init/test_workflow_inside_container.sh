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
echo "[1] apt-get base toolchain (python, curl, git, build tools)..."
apt-get update
apt-get install -y \
  python3.10 python3.10-dev python3-pip \
  cmake clang lld ninja-build binaryen libcriterion-dev \
  curl git build-essential ca-certificates pkg-config

echo ""
echo "[2] install uv (Python manager) and activate..."
curl -LsSf https://astral.sh/uv/install.sh | sh
export PATH="/root/.local/bin:${PATH}"
uv --version || echo "uv installed"

echo ""
echo "[3] install Rust toolchain..."
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
export PATH="/root/.cargo/bin:${PATH}"
rustup target add wasm32-wasip1

echo ""
echo "[4] clone repo, create uv venv and install Python deps..."
cd /tmp
rm -rf Lucid
git clone https://github.com/q2333gh/Lucid --depth=1
cd Lucid

echo "[4.1] create uv-managed virtualenv (.venv)..."
uv venv .venv

echo "[4.2] activate virtualenv..."
source .venv/bin/activate

echo "[4.3] install requirements into venv..."
uv pip install -r requirements.txt

echo ""
echo "[5] install WASI SDK..."
export WASI_SDK_VERSION=25.0
export WASI_SDK_ROOT=/opt/wasi-sdk/wasi-sdk-25.0-x86_64-linux
mkdir -p /opt/wasi-sdk
curl -L "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-25.0-x86_64-linux.tar.gz" \
  | tar -xz -C /opt/wasi-sdk

echo ""
echo "[6] create new example project..."
python3 build.py --new my_canister

echo ""
echo "[7] build IC wasm for my_canister..."
python3 build.py --icwasm --examples my_canister

