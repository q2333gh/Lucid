# First-Time Setup

Run the setup script:

```bash
./setup.sh
```

This installs required dependencies:
- Python 3.10+ and build tools
- uv (Python package manager)
- Python virtual environment
- WASI SDK

Optional: Rust + candid-extractor (for .did file generation)

After setup, add to your shell profile:
```bash
export WASI_SDK_ROOT="$HOME/opt/wasi-sdk/wasi-sdk-25.0-x86_64-linux"
export PATH="$HOME/.local/bin:$PATH"
```

Then:
```bash
source .venv/bin/activate
python build.py --new my_canister
python build.py --icwasm --examples adder
```
