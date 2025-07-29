#!/usr/bin/env python3
"""
WASM Add Function - Modern Python Build Script

A clean, maintainable replacement for build.sh with better error handling,
cross-platform support, and more readable code.

Usage:
    python build.py --minimal --test
    python build.py --full --wat
    python build.py --clean
"""

import argparse
import subprocess
import sys
import os
import shutil
from pathlib import Path
from typing import Optional, List
from dataclasses import dataclass
from enum import Enum

class BuildMode(Enum):
    MINIMAL = "minimal"
    FULL = "full"

class Color:
    """ANSI color codes for terminal output"""
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    BLUE = '\033[0;34m'
    YELLOW = '\033[1;33m'
    BOLD = '\033[1m'
    RESET = '\033[0m'
    
    @classmethod
    def colorize(cls, text: str, color: str) -> str:
        """Add color to text if stdout is a terminal"""
        if sys.stdout.isatty():
            return f"{color}{text}{cls.RESET}"
        return text

@dataclass
class BuildConfig:
    """Build configuration settings"""
    mode: BuildMode
    build_dir: Path
    wasm_file: str
    wat_file: str
    meson_options: List[str]

class WasmBuilder:
    """Main builder class for WASM projects"""
    
    def __init__(self):
        self.project_root = Path.cwd()
        self.configs = {
            BuildMode.MINIMAL: BuildConfig(
                mode=BuildMode.MINIMAL,
                build_dir=self.project_root / "build_minimal",
                wasm_file="add_minimal.wasm",
                wat_file="add_minimal.wat",
                meson_options=["-Dminimal=true"]
            ),
            BuildMode.FULL: BuildConfig(
                mode=BuildMode.FULL,
                build_dir=self.project_root / "build_full",
                wasm_file="add.wasm",
                wat_file="add.wat",
                meson_options=["-Dminimal=false"]
            )
        }
    
    def print_status(self, message: str, color: str = Color.BLUE) -> None:
        """Print colored status message"""
        print(Color.colorize(f"🚀 {message}", color))
    
    def print_success(self, message: str) -> None:
        """Print success message"""
        print(Color.colorize(f"✅ {message}", Color.GREEN))
    
    def print_error(self, message: str) -> None:
        """Print error message"""
        print(Color.colorize(f"❌ {message}", Color.RED), file=sys.stderr)
    
    def print_warning(self, message: str) -> None:
        """Print warning message"""
        print(Color.colorize(f"⚠️  {message}", Color.YELLOW))
    
    def run_command(self, cmd: List[str], cwd: Optional[Path] = None, 
                   capture_output: bool = False) -> subprocess.CompletedProcess:
        """Run a command with proper error handling"""
        try:
            result = subprocess.run(
                cmd, 
                cwd=cwd, 
                check=True, 
                capture_output=capture_output,
                text=True
            )
            return result
        except subprocess.CalledProcessError as e:
            self.print_error(f"Command failed: {' '.join(cmd)}")
            if e.stdout:
                print(e.stdout)
            if e.stderr:
                print(e.stderr, file=sys.stderr)
            sys.exit(1)
        except FileNotFoundError:
            self.print_error(f"Command not found: {cmd[0]}")
            self.print_warning("Please ensure all required tools are installed")
            sys.exit(1)
    
    def check_dependencies(self) -> None:
        """Check if required tools are available"""
        required_tools = ["meson", "ninja"]
        optional_tools = ["wasmtime", "wasm2wat"]
        
        missing_required = []
        missing_optional = []
        
        for tool in required_tools:
            if not shutil.which(tool):
                missing_required.append(tool)
        
        for tool in optional_tools:
            if not shutil.which(tool):
                missing_optional.append(tool)
        
        if missing_required:
            self.print_error(f"Missing required tools: {', '.join(missing_required)}")
            print("\nInstall them with:")
            print("  pip install meson ninja")
            sys.exit(1)
        
        if missing_optional:
            self.print_warning(f"Missing optional tools: {', '.join(missing_optional)}")
            print("Some features may not work. Install with:")
            if "wasmtime" in missing_optional:
                print("  curl https://wasmtime.dev/install.sh -sSf | bash")
            if "wasm2wat" in missing_optional:
                print("  # Install wabt tools for wasm2wat")
    
    def clean_build_dirs(self) -> None:
        """Clean all build directories"""
        self.print_status("Cleaning build directories...")
        
        dirs_to_clean = ["build_minimal", "build_full"]
        cleaned = []
        
        for dir_name in dirs_to_clean:
            dir_path = self.project_root / dir_name
            if dir_path.exists():
                shutil.rmtree(dir_path)
                cleaned.append(dir_name)
        
        if cleaned:
            self.print_success(f"Cleaned: {', '.join(cleaned)}")
        else:
            self.print_warning("No build directories to clean")
    
    def setup_build(self, config: BuildConfig) -> None:
        """Setup meson build directory"""
        if config.build_dir.exists():
            self.print_status(f"Reconfiguring existing build: {config.build_dir.name}")
            cmd = ["meson", "configure", str(config.build_dir)] + config.meson_options
        else:
            self.print_status(f"Setting up new build: {config.build_dir.name}")
            cmd = ["meson", "setup", str(config.build_dir)] + config.meson_options
        
        self.run_command(cmd)
    
    def compile_project(self, config: BuildConfig) -> None:
        """Compile the project"""
        self.print_status("Building...")
        cmd = ["meson", "compile", "-C", str(config.build_dir)]
        self.run_command(cmd)
        
        wasm_path = config.build_dir / config.wasm_file
        if wasm_path.exists():
            size = wasm_path.stat().st_size
            if size < 1024:
                size_str = f"{size} bytes"
            else:
                size_str = f"{size/1024:.1f} KB"
            self.print_success(f"Generated: {wasm_path} ({size_str})")
        else:
            self.print_error(f"WASM file not found: {wasm_path}")
            sys.exit(1)
    
    def run_tests(self, config: BuildConfig) -> None:
        """Run meson tests"""
        self.print_status("Running tests...")
        cmd = ["meson", "test", "-C", str(config.build_dir), "--verbose"]
        self.run_command(cmd)
        self.print_success("All tests passed!")
    
    def generate_wat(self, config: BuildConfig) -> None:
        """Generate WAT file"""
        self.print_status("Generating WAT file...")
        cmd = ["meson", "compile", "-C", str(config.build_dir), "wat"]
        self.run_command(cmd)
        
        wat_path = config.build_dir / config.wat_file
        if wat_path.exists():
            with open(wat_path, 'r') as f:
                line_count = sum(1 for _ in f)
            self.print_success(f"Generated: {wat_path} ({line_count} lines)")
        else:
            self.print_warning("WAT file generation may have failed")
    
    def build(self, mode: BuildMode, test: bool = False, wat: bool = False) -> None:
        """Main build function"""
        config = self.configs[mode]
        
        self.print_status(f"Building {mode.value} version...", Color.BOLD)
        
        # Setup and compile
        self.setup_build(config)
        self.compile_project(config)
        
        # Optional steps
        if test:
            self.run_tests(config)
        
        if wat:
            self.generate_wat(config)
        
        self.print_success("Build complete! 🎉")

def create_parser() -> argparse.ArgumentParser:
    """Create command line argument parser"""
    parser = argparse.ArgumentParser(
        description="WASM Add Function - Modern Build System",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --minimal --test     # Build minimal version and run tests
  %(prog)s --full --wat         # Build full version and generate WAT
  %(prog)s --clean              # Clean all build directories
  %(prog)s --minimal --test --wat  # Build, test, and generate WAT
        """
    )
    
    # Build mode (mutually exclusive)
    mode_group = parser.add_mutually_exclusive_group()
    mode_group.add_argument(
        "-m", "--minimal", 
        action="store_true",
        help="Build minimal WASM without WASI runtime"
    )
    mode_group.add_argument(
        "-f", "--full",
        action="store_true", 
        help="Build full WASI version (default)"
    )
    
    # Actions
    parser.add_argument(
        "-c", "--clean",
        action="store_true",
        help="Clean build directories"
    )
    parser.add_argument(
        "-t", "--test",
        action="store_true",
        help="Run tests after building"
    )
    parser.add_argument(
        "-w", "--wat",
        action="store_true",
        help="Generate WAT file after building"
    )
    
    return parser

def main():
    """Main entry point"""
    parser = create_parser()
    args = parser.parse_args()
    
    builder = WasmBuilder()
    
    # Handle clean operation
    if args.clean:
        builder.clean_build_dirs()
        return
    
    # Check dependencies
    builder.check_dependencies()
    
    # Determine build mode
    if args.minimal:
        mode = BuildMode.MINIMAL
    else:
        mode = BuildMode.FULL  # Default to full
    
    # Build the project
    try:
        builder.build(mode, test=args.test, wat=args.wat)
    except KeyboardInterrupt:
        builder.print_error("Build interrupted by user")
        sys.exit(1)
    except Exception as e:
        builder.print_error(f"Unexpected error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()