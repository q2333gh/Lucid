#!/bin/bash

# Meson build script for WASM add function
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="full"
CLEAN=false
TEST=false
WAT=false
HELP=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    -m|--minimal)
      BUILD_TYPE="minimal"
      shift
      ;;
    -f|--full)
      BUILD_TYPE="full"
      shift
      ;;
    -c|--clean)
      CLEAN=true
      shift
      ;;
    -t|--test)
      TEST=true
      shift
      ;;
    -w|--wat)
      WAT=true
      shift
      ;;
    -h|--help)
      HELP=true
      shift
      ;;
    *)
      echo "Unknown option $1"
      HELP=true
      shift
      ;;
  esac
done

# Show help
if [ "$HELP" = true ]; then
  echo -e "${BLUE}WASM Add Function - Meson Build Script${NC}"
  echo ""
  echo "Usage: $0 [OPTIONS]"
  echo ""
  echo "Options:"
  echo "  -m, --minimal     Build minimal WASM without WASI runtime"
  echo "  -f, --full        Build full WASI version (default)"
  echo "  -c, --clean       Clean build directories"
  echo "  -t, --test        Run tests after building"
  echo "  -w, --wat         Generate WAT file after building"
  echo "  -h, --help        Show this help message"
  echo ""
  echo "Examples:"
  echo "  $0                # Build full WASI version"
  echo "  $0 -m             # Build minimal version"
  echo "  $0 -m -t          # Build minimal and run tests"
  echo "  $0 -f -t -w       # Build full, test, and generate WAT"
  echo "  $0 -c             # Clean all build directories"
  exit 0
fi

# Clean if requested
if [ "$CLEAN" = true ]; then
  echo -e "${YELLOW}Cleaning build directories...${NC}"
  rm -rf build_full build_minimal
  echo -e "${GREEN}Clean complete.${NC}"
  exit 0
fi

# Set build directory and options based on build type
if [ "$BUILD_TYPE" = "minimal" ]; then
  BUILD_DIR="build_minimal"
  MESON_OPTIONS="-Dminimal=true"
  echo -e "${BLUE}Building minimal WASM version...${NC}"
else
  BUILD_DIR="build_full"
  MESON_OPTIONS="-Dminimal=false"
  echo -e "${BLUE}Building full WASI version...${NC}"
fi

# Setup build directory
if [ ! -d "$BUILD_DIR" ]; then
  echo -e "${YELLOW}Setting up build directory: $BUILD_DIR${NC}"
  meson setup "$BUILD_DIR" $MESON_OPTIONS
else
  echo -e "${YELLOW}Reconfiguring existing build directory: $BUILD_DIR${NC}"
  meson configure "$BUILD_DIR" $MESON_OPTIONS
fi

# Build
echo -e "${YELLOW}Building...${NC}"
meson compile -C "$BUILD_DIR"

# Show build results
echo -e "${GREEN}Build complete!${NC}"
if [ "$BUILD_TYPE" = "minimal" ]; then
  WASM_FILE="$BUILD_DIR/add_minimal.wasm"
else
  WASM_FILE="$BUILD_DIR/add.wasm"
fi

if [ -f "$WASM_FILE" ]; then
  echo -e "${GREEN}Generated: $WASM_FILE ($(du -h "$WASM_FILE" | cut -f1))${NC}"
else
  echo -e "${RED}Error: WASM file not found at $WASM_FILE${NC}"
  exit 1
fi

# Run tests if requested
if [ "$TEST" = true ]; then
  echo -e "${BLUE}Running tests...${NC}"
  meson test -C "$BUILD_DIR" --verbose
fi

# Generate WAT if requested
if [ "$WAT" = true ]; then
  echo -e "${BLUE}Generating WAT file...${NC}"
  meson compile -C "$BUILD_DIR" wat
  if [ "$BUILD_TYPE" = "minimal" ]; then
    WAT_FILE="$BUILD_DIR/add_minimal.wat"
  else
    WAT_FILE="$BUILD_DIR/add.wat"
  fi
  if [ -f "$WAT_FILE" ]; then
    echo -e "${GREEN}Generated: $WAT_FILE${NC}"
    echo -e "${YELLOW}WAT file has $(wc -l < "$WAT_FILE") lines${NC}"
  fi
fi

echo -e "${GREEN}All done!${NC}"