#!/usr/bin/env python3
"""
Build script - IC C SDK library build tool
Supports native and WASI platform compilation
"""

import sys
import shutil
from pathlib import Path

import typer
from rich.console import Console

# Import build utilities
from build_utils import (
    initialize_paths,
    get_wasi_sdk_paths,
    get_compile_flags,
    LIB_SOURCES,
    get_library_name,
    IC_WASI_POLYFILL_COMMIT,
    WASI2IC_COMMIT,
    NATIVE_C,
    NATIVE_AR,
    run_command,
    ensure_polyfill_library,
    ensure_wasi2ic_tool,
    check_source_files_exist,
    needs_rebuild,
    verify_raw_init_import,
    optimize_wasm,
)


########################################################################
console = Console(force_terminal=True, markup=True)
print = console.print  # route legacy prints through Rich for consistent styling


########################################################################
# Initialize Paths
########################################################################

SCRIPT_DIR = Path(__file__).parent.resolve()
PATHS = initialize_paths(SCRIPT_DIR)

# Extract paths for convenience
PROJECT_ROOT = PATHS['PROJECT_ROOT']
SRC_DIR = PATHS['SRC_DIR']
INCLUDE_DIR = PATHS['INCLUDE_DIR']
EXAMPLES_DIR = PATHS['EXAMPLES_DIR']
SCRIPTS_DIR = PATHS['SCRIPTS_DIR']
BUILD_DIR = PATHS['BUILD_DIR']
WASI_BUILD_DIR = PATHS['WASI_BUILD_DIR']

# Build script paths
BUILD_POLYFILL_SCRIPT = SCRIPT_DIR / "build_libic_wasi_polyfill.py"
BUILD_WASI2IC_SCRIPT = SCRIPT_DIR / "build_wasi2ic.py"

# Example files
EXAMPLE_SOURCES = list(EXAMPLES_DIR.glob("*.c")) if EXAMPLES_DIR.exists() else []

# Typer app
app = typer.Typer(add_completion=False, help="IC C SDK build tool")


########################################################################
# ICBuilder Class
########################################################################

class ICBuilder:
    """Builder class for IC C SDK library"""
    
    def __init__(self, target_platform="native"):
        self.target_platform = target_platform
        self.polyfill_library = None
        self.wasi2ic_tool = None
        
        # Get WASI SDK paths if needed
        if target_platform == "wasi":
            wasi_paths = get_wasi_sdk_paths()
            self.cc = str(wasi_paths['WASI_C'])
            self.ar = str(wasi_paths['WASI_AR'])
            self.wasi_sdk_compiler_root = wasi_paths['WASI_SDK_COMPILER_ROOT']
            self.build_dir = WASI_BUILD_DIR
        else:  # native
            self.cc = NATIVE_C
            self.ar = NATIVE_AR
            self.wasi_sdk_compiler_root = None
            self.build_dir = BUILD_DIR
        
        # Get compilation flags
        flags = get_compile_flags(INCLUDE_DIR, target_platform)
        self.cflags = flags['CFLAGS'].copy()
        self.ldflags = flags['LDFLAGS'].copy()
        
        # Library configuration
        self.lib_name = get_library_name(target_platform)
        self.lib_path = self.build_dir / self.lib_name
    
    def ensure_directories(self):
        """Ensure build directories exist"""
        self.build_dir.mkdir(exist_ok=True)
    
    def compile_source(self, source_file: str, source_dir: Path) -> bool:
        """Compile a single source file"""
        obj_file = self.build_dir / source_file.replace(".c", ".o")
        cmd = [self.cc] + self.cflags + ["-c", str(source_dir / source_file), "-o", str(obj_file)]
        
        # Disable LTO for ic_wasi_polyfill.c to prevent removal of raw_init import
        if source_file == "ic_wasi_polyfill.c" and self.target_platform == "wasi":
            # Remove LTO flags for this file
            cmd = [self.cc] + [f for f in self.cflags if not f.startswith("-flto")] + \
                  ["-c", str(source_dir / source_file), "-o", str(obj_file)]
        
        return run_command(cmd, f"Compiling {source_file}")
    
    def create_static_lib(self, object_files: list) -> bool:
        """Create static library"""
        objects_str = [str(obj) for obj in object_files]
        cmd = [self.ar, "rcs", str(self.lib_path)] + objects_str
        return run_command(cmd, f"Creating static library {self.lib_name}")
    
    def build_library(self) -> bool:
        """Build library"""
        platform_name = "WASI" if self.target_platform == "wasi" else "Native"
        print(f"[bold cyan]Starting IC C SDK library build ({platform_name})[/]")
        
        # Check if WASI SDK exists
        if self.target_platform == "wasi":
            wasi_paths = get_wasi_sdk_paths()
            if not wasi_paths['WASI_C'].exists():
                print(f"[red]Error: WASI SDK not found: {wasi_paths['WASI_C']}[/]")
                print(f"   Please install WASI SDK to: {wasi_paths['WASI_SDK_COMPILER_ROOT']}")
                print(f"   Or set environment variable WASI_SDK_ROOT")
                return False
        
        # Ensure directories exist
        self.ensure_directories()
        
        # Check if source files exist
        all_exist, missing_files = check_source_files_exist(SRC_DIR, LIB_SOURCES)
        if not all_exist:
            print("[red]Missing source files:[/]")
            for file in missing_files:
                print(f"   • {file}")
            return False
        
        # Compile all library source files
        print("\n[bold]Compiling library source files...[/]")
        object_files = []
        for source in LIB_SOURCES:
            obj_file = self.build_dir / source.replace(".c", ".o")
            if not self.compile_source(source, SRC_DIR):
                return False
            object_files.append(obj_file)
        
        # Create static library
        print("\n[bold]Creating static library...[/]")
        if not self.create_static_lib(object_files):
            return False
        
        print(f"\n[green]Library build successful! Static library: {self.lib_path}[/]")
        return True
    
    def ensure_polyfill_library(self) -> bool:
        """Ensure libic_wasi_polyfill.a exists, build it if needed"""
        if self.polyfill_library and self.polyfill_library.exists():
            return True
        
        polyfill_path = ensure_polyfill_library(SCRIPTS_DIR, BUILD_POLYFILL_SCRIPT)
        if polyfill_path:
            self.polyfill_library = polyfill_path
            return True
        return False
    
    def ensure_wasi2ic_tool(self) -> bool:
        """Ensure wasi2ic tool exists, build it if needed"""
        if self.wasi2ic_tool and self.wasi2ic_tool.exists():
            return True
        
        wasi2ic_path = ensure_wasi2ic_tool(SCRIPTS_DIR, BUILD_WASI2IC_SCRIPT)
        if wasi2ic_path:
            self.wasi2ic_tool = wasi2ic_path
            return True
        return False
    
    def build_examples(self) -> bool:
        """Build example programs"""
        if not EXAMPLE_SOURCES:
            print("[yellow]No example files found[/]")
            return True
        
        # Check if library needs rebuild
        if needs_rebuild(self.lib_path, SRC_DIR, LIB_SOURCES, self.target_platform, self.build_dir):
            print("[yellow]Library needs rebuild, auto-building library...[/]")
            if not self.build_library():
                print("[red]Library build failed, cannot continue building examples[/]")
                return False
            print()
        
        platform_name = "WASI" if self.target_platform == "wasi" else "Native"
        print(f"\n[bold]Building example programs ({platform_name})...[/]")
        
        success = True
        for example_src in EXAMPLE_SOURCES:
            example_name = example_src.stem
            obj_file = self.build_dir / f"{example_name}.o"
            
            # Compile example as object file
            cmd = [self.cc] + self.cflags + ["-c", str(example_src), "-o", str(obj_file)]
            if not run_command(cmd, f"Compiling example {example_name}"):
                success = False
                continue
            
            # For WASI platform, link to generate .wasm file
            if self.target_platform == "wasi":
                # Ensure polyfill library exists
                if not self.ensure_polyfill_library():
                    print(f"[red]  Error: Cannot proceed without libic_wasi_polyfill.a[/]")
                    success = False
                    continue
                
                wasi_wasm_target = self.build_dir / f"{example_name}_wasi.wasm"
                ic_wasm_target = self.build_dir / f"{example_name}.wasm"
                
                # Step 1: Link to generate WASI WASM (includes libic_wasi_polyfill.a)
                print(f"\n  [bold]Linking with IC WASI Polyfill library:[/]")
                print(f"     Path: {self.polyfill_library.resolve()}")
                
                # Remove LTO from link flags to prevent removal of raw_init import
                link_flags = [f for f in self.ldflags if not f.startswith("-Wl,--lto")]
                cmd = [self.cc] + [str(obj_file), str(self.lib_path), str(self.polyfill_library)] + \
                      link_flags + ["-Wl,--no-entry", "-Wl,--export-all", "-Wl,--import-undefined", 
                                   "-Wl,--undefined=raw_init", "-o", str(wasi_wasm_target)]
                
                if not run_command(cmd, f"Linking to generate WASI WASM {example_name}"):
                    success = False
                    continue
                
                # Verify that raw_init import is present in the linked WASM file
                print(f"\n  [bold]Verifying raw_init import linkage...[/]")
                if not verify_raw_init_import(wasi_wasm_target, self.wasi_sdk_compiler_root):
                    print(f"[yellow]  Warning: raw_init import verification failed for {example_name}[/]")
                    success = False
                
                # Step 2: Use wasi2ic to convert to IC-compatible WASM
                if not self.ensure_wasi2ic_tool():
                    print(f"[red]  Error: Cannot proceed without wasi2ic tool[/]")
                    success = False
                    continue
                
                print(f"\n  [bold]Using wasi2ic tool:[/]")
                print(f"     Path: {self.wasi2ic_tool.resolve()}")
                
                wasi2ic_cmd = [str(self.wasi2ic_tool), str(wasi_wasm_target), str(ic_wasm_target)]
                if run_command(wasi2ic_cmd, f"Converting {example_name} to IC-compatible version"):
                    print(f"[green]  {example_name}: {ic_wasm_target} (IC compatible)[/]")
                    print(f"     (WASI version: {wasi_wasm_target})")
                    
                    # Step 3: Optimize WASM file with wasm-opt
                    optimized_wasm_target = self.build_dir / f"{example_name}_optimized.wasm"
                    print(f"\n  [bold]Optimizing WASM file...[/]")
                    if optimize_wasm(ic_wasm_target, optimized_wasm_target):
                        print(f"[green]  Optimized version: {optimized_wasm_target}[/]")
                    else:
                        # Optimization is optional, don't fail the build
                        print(f"[cyan]  Original version available: {ic_wasm_target}[/]")
                else:
                    print(f"[yellow]  wasi2ic conversion failed, but WASI WASM generated: {wasi_wasm_target}[/]")
                    success = False
            else:
                # Native platform only compiles object files
                print(f"[green]  {example_name}: {obj_file} (object file)[/]")
                print(f"     Note: Native platform requires separate test program to call these functions")
        
        return success
    
    def clean(self):
        """Clean build artifacts"""
        print("[bold]Cleaning build artifacts...[/]")
        
        removed_count = 0
        for build_path in [BUILD_DIR, WASI_BUILD_DIR]:
            if build_path.exists():
                try:
                    shutil.rmtree(build_path)
                    print(f"[green]  Deleted directory: {build_path}[/]")
                    removed_count += 1
                except OSError as e:
                    print(f"[red]  Cannot delete {build_path}: {e}[/]")
        
        if removed_count == 0:
            print("[cyan]  No directories to clean[/]")
        else:
            print(f"[green]  Cleaned {removed_count} directory(ies)[/]")
    
    def show_info(self):
        """Show build information"""
        platform_name = "WASI" if self.target_platform == "wasi" else "Native"
        print("📋 Build configuration information:")
        print(f"  Target platform: {platform_name}")
        print(f"  Compiler: {self.cc}")
        print(f"  Archiver: {self.ar}")
        print(f"  Compilation options: {' '.join(self.cflags)}")
        if self.ldflags:
            print(f"  Linking options: {' '.join(self.ldflags)}")
        print(f"  Build directory: {self.build_dir}")
        print(f"  Library file: {self.lib_path}")
        
        print("\n  Library source files:")
        for src in LIB_SOURCES:
            src_path = SRC_DIR / src
            status = "[green]OK[/]" if src_path.exists() else "[red]Missing[/]"
            print(f"    {status} {src}")
        
        if EXAMPLE_SOURCES:
            print("\n  Example files:")
            for example in EXAMPLE_SOURCES:
                print(f"    • {example.name}")
        
        if self.target_platform == "wasi":
            wasi_paths = get_wasi_sdk_paths()
            print(f"\n  WASI SDK: {wasi_paths['WASI_SDK_COMPILER_ROOT']}")
            print(f"  System root: {wasi_paths['WASI_SYSROOT']}")
            print(f"\n  IC WASI Polyfill (locked commit: {IC_WASI_POLYFILL_COMMIT}):")
            polyfill_path = ensure_polyfill_library(SCRIPTS_DIR, BUILD_POLYFILL_SCRIPT)
            if polyfill_path:
                print(f"    Library: {polyfill_path}")
                print(f"    Status: [green]Found[/]")
            else:
                print(f"    Status: ✗ Not found")
            print(f"    Build script: {BUILD_POLYFILL_SCRIPT}")
            
            print(f"\n  wasi2ic tool (locked commit: {WASI2IC_COMMIT}):")
            wasi2ic_path = ensure_wasi2ic_tool(SCRIPTS_DIR, BUILD_WASI2IC_SCRIPT)
            if wasi2ic_path:
                print(f"    Path: {wasi2ic_path}")
                print(f"    Status: [green]Found[/]")
            else:
                print(f"    Status: ✗ Not found")
                print(f"    Note: Set WASI2IC_PATH environment variable to specify custom path")


########################################################################
# Main Entry Point
########################################################################

@app.command(context_settings={"help_option_names": ["-h", "--help"]})
def run(
    build: bool = typer.Option(
        False,
        "--build",
        "-b",
        help="Build the IC C SDK library",
    ),
    clean: bool = typer.Option(
        False,
        "--clean",
        "-c",
        help="Remove build artifacts",
    ),
    examples: bool = typer.Option(
        False,
        "--examples",
        "-e",
        help="Build example programs",
    ),
    info: bool = typer.Option(
        False,
        "--info",
        "-i",
        help="Show build configuration",
    ),
    wasi: bool = typer.Option(
        False,
        "--wasi",
        "-w",
        help="Target the WASI toolchain (default is native)",
    ),
):
    """
    Manage IC C SDK builds. If no action flags are provided, --build is assumed.
    """
    # Default to building when no action was specified
    if not any([build, clean, examples, info]):
        build = True

    target_platform = "wasi" if wasi else "native"
    builder = ICBuilder(target_platform)
    success = True

    if info:
        builder.show_info()
        print()

    if clean:
        builder.clean()
        print()

    if build:
        if not builder.build_library():
            success = False
        print()

    if examples and success:
        if not builder.build_examples():
            success = False
        print()

    raise typer.Exit(code=0 if success else 1)


if __name__ == "__main__":
    app()
