# WASM Add Function - Modern Build System

A clean, maintainable Python build script for WASM projects with beautiful terminal output and modern dependency management.

## Features

- 🚀 **Modern CLI** - Built with Typer for type-safe command-line interface
- 🎨 **Beautiful Output** - Rich terminal output with colors, tables, and progress bars  
- 🔧 **Dependency Management** - Uses `uv` for fast Python package management
- ✅ **Data Validation** - Pydantic models for configuration validation
- 🛠️ **Command Execution** - Invoke library for better subprocess handling
- 📊 **Progress Tracking** - Real-time build progress indicators

## Prerequisites

- Python 3.8+
- [uv](https://github.com/astral-sh/uv) - Fast Python package installer and resolver
- Meson build system
- Ninja build backend

### Install uv

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

## Usage

### Using the Python script directly

```bash
# Show help
uv run python build.py --help

# Build minimal WASM (no WASI runtime)
uv run python build.py --minimal

# Build full WASI version (default)
uv run python build.py --full

# Build and run tests
uv run python build.py --minimal --test

# Build, test, and generate WAT file
uv run python build.py --full --test --wat

# Clean build directories
uv run python build.py --clean
```

### Using the shell wrapper

```bash
# The build.sh script is a convenient wrapper
./build.sh --help
./build.sh --minimal --test
./build.sh --clean
```

## Project Structure

```
.
├── build.py           # Modern Python build script
├── build.sh           # Shell wrapper for uv
├── pyproject.toml     # Python project configuration
├── requirements.txt   # Legacy requirements file
├── meson.build        # Meson build configuration
├── add.c              # C source code
└── README.md          # This file
```

## Dependencies

The build script uses modern Python libraries:

- **typer** - Modern CLI framework with rich integration
- **rich** - Beautiful terminal output with colors, tables, progress bars
- **pydantic** - Data validation and settings management  
- **invoke** - Python task execution library

These are automatically managed by `uv` and defined in `pyproject.toml`.

## Examples

### Basic build commands

```bash
# Clean previous builds
./build.sh --clean

# Build minimal version and run tests
./build.sh --minimal --test

# Build full version with WAT output
./build.sh --full --wat

# Build everything
./build.sh --full --test --wat
```

### Output examples

The modern build script provides beautiful terminal output:

- ✅ Colored success/error messages
- 🚀 Progress indicators during builds
- 📊 Dependency status tables
- ⚠️ Warning panels for missing optional tools

## Migration from old build.sh

The old bash-based `build.sh` has been replaced with a modern Python implementation. The command-line interface is compatible:

| Old Command | New Command |
|-------------|-------------|
| `./build.sh -m` | `./build.sh --minimal` |
| `./build.sh -f` | `./build.sh --full` |
| `./build.sh -t` | `./build.sh --test` |
| `./build.sh -w` | `./build.sh --wat` |
| `./build.sh -c` | `./build.sh --clean` |

## Development

To work on the build script itself:

```bash
# Install development dependencies
uv sync

# Run with development environment
uv run python build.py --help

# Format code
uv run black build.py

# Lint code  
uv run ruff check build.py
```