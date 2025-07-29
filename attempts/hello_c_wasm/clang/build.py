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

import sys
import os
import shutil
from pathlib import Path
from typing import Optional, List
from enum import Enum

# Modern libraries for better UX and maintainability
import typer
from rich.console import Console
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn
from rich.table import Table
from pydantic import BaseModel, field_validator
from invoke import Context

class BuildMode(Enum):
    MINIMAL = "minimal"
    FULL = "full"

# Initialize Rich console for beautiful output
console = Console()

class BuildConfig(BaseModel):
    """Build configuration settings with validation"""
    mode: BuildMode
    build_dir: Path
    wasm_file: str
    wat_file: str
    meson_options: List[str]
    
    @field_validator('build_dir')
    def validate_build_dir(cls, v):
        """Ensure build_dir is a resolved Path object"""
        return Path(v).resolve()
    
    @field_validator('wasm_file', 'wat_file')
    def validate_filenames(cls, v):
        """Ensure filenames have proper extensions"""
        if not v:
            raise ValueError("Filename cannot be empty")
        return v

class WasmBuilder:
    """Main builder class for WASM projects"""
    
    def __init__(self, verbose: bool = False):
        self.project_root = Path.cwd()
        self.verbose = verbose
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
    
    def print_status(self, message: str, color: str = "blue") -> None:
        """Print colored status message"""
        console.print(f"🚀 {message}", style=color)
    
    def print_success(self, message: str) -> None:
        """Print success message"""
        console.print(f"✅ {message}", style="green")
    
    def print_error(self, message: str) -> None:
        """Print error message"""
        # Rich console doesn't support file parameter, so we print to stderr differently
        import sys
        stderr_console = Console(file=sys.stderr)
        stderr_console.print(f"❌ {message}", style="red")
    
    def print_warning(self, message: str) -> None:
        """Print warning message"""
        console.print(f"⚠️  {message}", style="yellow")
    
    def print_command_header(self, cmd: str, cwd: Optional[Path] = None) -> None:
        """Print a nicely formatted command header for verbose mode"""
        console.print()  # Empty line for spacing
        
        # Truncate long commands for display
        display_cmd = cmd if len(cmd) <= 50 else cmd[:47] + "..."
        
        console.print("┌" + "─" * 60 + "┐", style="dim blue")
        console.print(f"│ [bold blue]Command:[/bold blue] {display_cmd:<47} │", style="dim blue")
        if len(cmd) > 50:
            console.print(f"│ [dim]Full: {cmd}[/dim]", style="dim blue")
        if cwd:
            console.print(f"│ [dim]Directory: {str(cwd)}[/dim]", style="dim blue")
        console.print("└" + "─" * 60 + "┘", style="dim blue")
    
    def print_command_footer(self, success: bool = True) -> None:
        """Print command completion status"""
        if success:
            console.print("[dim green]✓ Command completed successfully[/dim green]")
        console.print()  # Empty line for spacing
    
    def run_command(self, cmd: str, cwd: Optional[Path] = None) -> None:
        """Run a command with proper error handling using Invoke"""
        try:
            if self.verbose:
                # Show formatted command header
                self.print_command_header(cmd, cwd)
                
                # Run with visible output in verbose mode
                ctx = Context()
                if cwd:
                    with ctx.cd(str(cwd)):
                        result = ctx.run(cmd, warn=True, hide=False)
                else:
                    result = ctx.run(cmd, warn=True, hide=False)
                
                if result.failed:
                    self.print_command_footer(success=False)
                    self.print_error(f"Command failed with exit code {result.exited}: {cmd}")
                    sys.exit(1)
                else:
                    self.print_command_footer(success=True)
            else:
                # Original behavior with hidden output and spinner
                with console.status(f"[bold blue]Running: {cmd}"):
                    ctx = Context()
                    if cwd:
                        with ctx.cd(str(cwd)):
                            result = ctx.run(cmd, warn=True, hide=True)
                    else:
                        result = ctx.run(cmd, warn=True, hide=True)
                    
                    if result.failed:
                        self.print_error(f"Command failed: {cmd}")
                        if result.stdout:
                            console.print(result.stdout)
                        if result.stderr:
                            console.print(result.stderr, style="red")
                        sys.exit(1)
        except Exception as e:
            self.print_error(f"Failed to execute command: {cmd}")
            self.print_error(f"Error: {e}")
            sys.exit(1)
    
    def check_dependencies(self) -> None:
        """Check if required tools are available with Rich output"""
        required_tools = ["meson", "ninja"]
        optional_tools = ["wasmtime", "wasm2wat"]
        
        # Create a table for dependency status
        table = Table(title="Dependency Check")
        table.add_column("Tool", style="cyan")
        table.add_column("Status", style="magenta")
        table.add_column("Type", style="green")
        
        missing_required = []
        missing_optional = []
        
        for tool in required_tools:
            if shutil.which(tool):
                table.add_row(tool, "✅ Found", "Required")
            else:
                table.add_row(tool, "❌ Missing", "Required")
                missing_required.append(tool)
        
        for tool in optional_tools:
            if shutil.which(tool):
                table.add_row(tool, "✅ Found", "Optional")
            else:
                table.add_row(tool, "⚠️ Missing", "Optional")
                missing_optional.append(tool)
        
        console.print(table)
        
        if missing_required:
            panel = Panel(
                "Missing required tools: " + ", ".join(missing_required) + "\n\n" +
                "Install them with:\n" +
                "  pip install meson ninja",
                title="❌ Missing Dependencies",
                border_style="red"
            )
            console.print(panel)
            sys.exit(1)
        
        if missing_optional:
            install_commands = []
            if "wasmtime" in missing_optional:
                install_commands.append("curl https://wasmtime.dev/install.sh -sSf | bash")
            if "wasm2wat" in missing_optional:
                install_commands.append("# Install wabt tools for wasm2wat")
            
            panel = Panel(
                "Missing optional tools: " + ", ".join(missing_optional) + "\n\n" +
                "Some features may not work. Install with:\n" +
                "\n".join(install_commands),
                title="⚠️ Optional Dependencies",
                border_style="yellow"
            )
            console.print(panel)
    
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
            cmd = f"meson configure {config.build_dir} {' '.join(config.meson_options)}"
        else:
            self.print_status(f"Setting up new build: {config.build_dir.name}")
            cmd = f"meson setup {config.build_dir} {' '.join(config.meson_options)}"
        
        self.run_command(cmd)
    
    def compile_project(self, config: BuildConfig) -> None:
        """Compile the project"""
        self.print_status("Building...")
        cmd = f"meson compile -C {config.build_dir}"
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
        cmd = f"meson test -C {config.build_dir} --verbose"
        self.run_command(cmd)
        self.print_success("All tests passed!")
    
    def generate_wat(self, config: BuildConfig) -> None:
        """Generate WAT file"""
        self.print_status("Generating WAT file...")
        cmd = f"meson compile -C {config.build_dir} wat"
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
        
        self.print_status(f"Building {mode.value} version...", "bold")
        
        with Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            console=console,
        ) as progress:
            # Setup and compile
            task = progress.add_task("Setting up build...", total=None)
            self.setup_build(config)
            
            progress.update(task, description="Compiling...")
            self.compile_project(config)
            
            # Optional steps
            if test:
                progress.update(task, description="Running tests...")
                self.run_tests(config)
            
            if wat:
                progress.update(task, description="Generating WAT...")
                self.generate_wat(config)
            
            progress.update(task, description="Complete!", completed=True)
        
        self.print_success("Build complete! 🎉")

# Initialize Typer app
app = typer.Typer(
    name="build",
    help="WASM Add Function - Modern Build System",
    add_completion=False,
    rich_markup_mode="rich"
)

@app.command()
def build_command(
    minimal: bool = typer.Option(False, "--minimal", "-m", help="Build minimal WASM without WASI runtime"),
    full: bool = typer.Option(False, "--full", "-f", help="Build full WASI version"),
    clean: bool = typer.Option(False, "--clean", "-c", help="Clean build directories"),
    test: bool = typer.Option(False, "--test", "-t", help="Run tests after building"),
    wat: bool = typer.Option(False, "--wat", "-w", help="Generate WAT file after building"),
    verbose: bool = typer.Option(False, "--verbose", "-v", help="Show detailed build logs and commands"),
):
    """
    Build WASM add function with various options.
    
    Examples:
    
    • [bold cyan]python build.py --minimal --test[/bold cyan]         # Build minimal version and run tests
    • [bold cyan]python build.py --full --wat[/bold cyan]             # Build full version and generate WAT  
    • [bold cyan]python build.py --clean[/bold cyan]                  # Clean all build directories
    • [bold cyan]python build.py --minimal --test --wat --verbose[/bold cyan]  # Build with detailed logs
    """
    
    builder = WasmBuilder(verbose=verbose)
    
    # Handle clean operation
    if clean:
        builder.clean_build_dirs()
        return
    
    # Check dependencies
    builder.check_dependencies()
    
    # Determine build mode
    if minimal and full:
        console.print("❌ Cannot specify both --minimal and --full", style="red")
        raise typer.Exit(1)
    elif minimal:
        mode = BuildMode.MINIMAL
    else:
        mode = BuildMode.FULL  # Default to full
    
    # Build the project
    try:
        builder.build(mode, test=test, wat=wat)
    except KeyboardInterrupt:
        builder.print_error("Build interrupted by user")
        raise typer.Exit(1)
    except Exception as e:
        builder.print_error(f"Unexpected error: {e}")
        raise typer.Exit(1)

if __name__ == "__main__":
    app()