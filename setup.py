#!/usr/bin/env python3
"""
Lucid CDK-C Setup Script
Assume OS: x86 ubuntu with sudo permit
"""
import os
import sys
import subprocess
import shutil
import urllib.request
import tarfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent.resolve()
HOME = Path.home()
BASHRC = HOME / ".bashrc"


def run_cmd(cmd, check=True, shell=False):
    """Run a command."""
    if isinstance(cmd, str) and not shell:
        cmd = cmd.split()
    subprocess.run(cmd, check=check, shell=shell, text=True)


def persist_export(pattern, export_line):
    """Persist export to .bashrc."""
    if not BASHRC.exists():
        BASHRC.write_text("")

    lines = BASHRC.read_text().splitlines()
    for i, line in enumerate(lines):
        if pattern in line:
            lines[i] = export_line
            BASHRC.write_text("\n".join(lines) + "\n")
            return

    lines.append(export_line)
    BASHRC.write_text("\n".join(lines) + "\n")


def ensure_build_tools():
    """Ensure build tools are installed."""
    if shutil.which("cmake") and shutil.which("clang") and shutil.which("ninja"):
        return

    run_cmd(
        [
            "sudo",
            "sh",
            "-c",
            "apt-get update && apt-get install -y --no-install-recommends cmake clang lld ninja-build binaryen build-essential && rm -rf /var/lib/apt/lists/*",
        ]
    )


def ensure_uv():
    """Ensure uv is installed."""
    local_bin = HOME / ".local" / "bin"
    if local_bin.exists():
        os.environ["PATH"] = f"{local_bin}:{os.environ.get('PATH', '')}"

    if shutil.which("uv"):
        return

    run_cmd("curl -LsSf https://astral.sh/uv/install.sh | sh", shell=True)
    if local_bin.exists():
        os.environ["PATH"] = f"{local_bin}:{os.environ.get('PATH', '')}"


def ensure_venv():
    """Ensure virtual environment exists."""
    venv_dir = SCRIPT_DIR / ".venv"
    if venv_dir.exists():
        return

    uv_cmd = shutil.which("uv") or str(HOME / ".local" / "bin" / "uv")
    run_cmd([uv_cmd, "venv", ".venv"])


def install_requirements():
    """Install Python requirements."""
    requirements_file = SCRIPT_DIR / "requirements.txt"
    if not requirements_file.exists():
        return

    uv_pip = shutil.which("uv") or str(HOME / ".local" / "bin" / "uv")
    run_cmd([uv_pip, "pip", "install", "-r", str(requirements_file)])


def ensure_wasi_sdk():
    """Ensure WASI SDK is installed and return its root path."""
    wasi_sdk_root = os.environ.get("WASI_SDK_ROOT")
    if wasi_sdk_root and Path(wasi_sdk_root).exists():
        return wasi_sdk_root

    WASI_SDK_VERSION = "25.0"
    INSTALL_DIR = HOME / "opt" / "wasi-sdk"
    SDK_DIR = f"wasi-sdk-{WASI_SDK_VERSION}-x86_64-linux"
    SDK_PATH = INSTALL_DIR / SDK_DIR

    if SDK_PATH.exists():
        return str(SDK_PATH)

    INSTALL_DIR.mkdir(parents=True, exist_ok=True)
    url = f"https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/{SDK_DIR}.tar.gz"
    tar_path = INSTALL_DIR / f"{SDK_DIR}.tar.gz"

    urllib.request.urlretrieve(url, tar_path)
    with tarfile.open(tar_path, "r:gz") as tar:
        tar.extractall(INSTALL_DIR)

    tar_path.unlink()
    return str(SDK_PATH)


def get_candid_extractor_download_url():
    """Get the download URL for candid-extractor binary."""
    return "https://github.com/q2333gh/Lucid/releases/download/candid-extractor/candid-extractor-linux-amd64.tar.gz"


def ensure_candid_extractor():
    """Ensure candid-extractor is installed and linked to /usr/local/bin."""
    install_dir = Path("/opt/candid-extractor")
    binary_path = install_dir / "candid-extractor-linux-amd64"
    symlink_path = Path("/usr/local/bin/candid-extractor")

    if binary_path.exists() and symlink_path.exists():
        return str(symlink_path)

    install_dir.mkdir(parents=True, exist_ok=True)

    url = get_candid_extractor_download_url()
    tar_path = install_dir / "candid-extractor-linux-amd64.tar.gz"

    print(f"Downloading candid-extractor from {url}...")
    urllib.request.urlretrieve(url, tar_path)

    with tarfile.open(tar_path, "r:gz") as tar:
        tar.extractall(install_dir)

    tar_path.unlink()
    binary_path.chmod(0o755)

    if symlink_path.exists():
        symlink_path.unlink()
    symlink_path.symlink_to(binary_path)

    print(f"✓ candid-extractor installed at {binary_path} and linked to {symlink_path}")
    return str(symlink_path)


def test_build(wasi_sdk_root):
    """Run build test: python build.py --icwasm --examples adder."""
    venv_python = SCRIPT_DIR / ".venv" / "bin" / "python"
    python_cmd = str(venv_python) if venv_python.exists() else sys.executable

    os.environ["WASI_SDK_ROOT"] = wasi_sdk_root
    run_cmd([python_cmd, "build.py", "--icwasm", "--examples", "adder"])


def main():
    os.chdir(SCRIPT_DIR)

    ensure_build_tools()
    wasi_sdk_root = ensure_wasi_sdk()
    persist_export("export WASI_SDK_ROOT=", f'export WASI_SDK_ROOT="{wasi_sdk_root}"')
    ensure_uv()
    ensure_venv()
    install_requirements()
    ensure_candid_extractor()

    test_build(wasi_sdk_root)

    print("✓ Setup complete!")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"Error: Command failed with exit code {e.returncode}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
