import sys
from pathlib import Path

# TODO xx.c name should be project_name.c
def create_new_project(project_name: str, root_dir: Path) -> None:
    """
    Create a new minimal hello world project template in examples directory.

    Args:
        project_name: Name of the project (and directory)
        root_dir: The root directory of the repository (where build.py resides)
    """
    examples_dir = root_dir / "examples"
    project_dir = examples_dir / project_name

    if project_dir.exists():
        print(f"Error: Project directory already exists: {project_dir}")
        sys.exit(1)

    print(f"Creating new project: {project_name}")
    project_dir.mkdir(parents=True)

    # 1. greet.c
    greet_c_content = """#include "ic_c_sdk.h"

// Export helper for candid-extractor
IC_CANDID_EXPORT_DID()

#include "idl/candid.h"

// Minimal Hello World
IC_API_QUERY(greet, "() -> (text)") {
    IC_API_REPLY_TEXT("Hello from minimal C canister!");
}
"""
    (project_dir / "greet.c").write_text(greet_c_content)

    # TODO also cmakelist.txt contents need the same project_name.c
    # 2. CMakeLists.txt
    cmake_content = f"""if(BUILD_TARGET_WASI)
    add_executable({project_name} greet.c)
    setup_ic_canister_target({project_name})
endif()
"""
    (project_dir / "CMakeLists.txt").write_text(cmake_content)

    # 3. .did file
    did_content = """service : {
    "greet": () -> (text) query;
}
"""
    (project_dir / f"{project_name}.did").write_text(did_content)

    print(f"✓ Project created at: {project_dir}")
    print(f"  Files: greet.c, CMakeLists.txt, {project_name}.did")
    print("\nTo build:")
    print(f"  python build.py --icwasm --examples {project_name}")
