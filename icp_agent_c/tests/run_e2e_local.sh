#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-main"
ADDER_DIR="${ROOT_DIR}/examples/adder"

if ! command -v dfx >/dev/null 2>&1; then
    echo "dfx not found in PATH"
    exit 1
fi

# Avoid proxy interference with local dfx/pocket-ic control plane calls.
unset HTTP_PROXY HTTPS_PROXY ALL_PROXY http_proxy https_proxy all_proxy
export NO_PROXY="127.0.0.1,localhost"
export no_proxy="${NO_PROXY}"
# Avoid dfx color-output panics in non-interactive shells.
export NO_COLOR=1
export CLICOLOR=0
export CLICOLOR_FORCE=0
export FORCE_COLOR=0

pushd "${ADDER_DIR}" >/dev/null
dfx stop >/dev/null 2>&1 || true
dfx start --clean --background
trap 'dfx stop >/dev/null 2>&1 || true' EXIT
dfx deploy adder
CANISTER_ID="$(dfx canister id adder)"
popd >/dev/null

cmake --build "${BUILD_DIR}" -j
ICP_AGENT_E2E_REPLICA_URL="http://127.0.0.1:4943" \
ICP_AGENT_E2E_CANISTER_ID="${CANISTER_ID}" \
ctest --test-dir "${BUILD_DIR}" -R icp_agent_c_e2e_local -V
