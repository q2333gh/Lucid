# Repository Guidelines

## Project Structure & Module Organization
- `cdk-c/include/` and `cdk-c/src/`: core C SDK headers and implementation.
- `examples/`: sample canisters (for example, `examples/hello_lucid/`).
- `build/` and `build-wasi/`: generated artifacts for native and WASI builds.
- `doc/`: developer and user documentation (see `doc/cdk-user/`).
- `test/` and `examples/*/test/`: tests and example-specific test scripts.

## Build, Test, and Development Commands
- `python build.py`: build the native library (used for local testing).
- `python build.py --icwasm`: build IC-compatible WASM output.
- `python build.py --new hello_lucid`: scaffold an example canister.
- `python examples/hello_lucid/test/t1_candid.py`: run the hello example test.
- `pip install -r requirements.txt`: install Python test dependencies.
- `pytest -s examples/types_example/te_test.py`: run PocketIC tests with output.

## Coding Style & Naming Conventions
- C formatting is driven by `.clang-format` (LLVM base, 4-space indent, 80-column limit).
- Prefer `snake_case` for C functions/variables and `UPPER_SNAKE_CASE` for macros.
- Keep headers in `cdk-c/include/` and update public APIs there first.
- Use `clang-format -i path/to/file.c` before committing.

## Testing Guidelines
- Python tests use `pytest` with PocketIC; see `examples/types_example/te_test.py`.
- Example tests usually live under `examples/<name>/test/`.
- Name pytest tests as `test_*` and keep fixtures in the same file unless shared.

## Commit & Pull Request Guidelines
- Follow conventional commits: `type(scope): message` (for example, `docs(core): ...`).
- Keep commits focused and update docs/examples when behavior changes.
- PRs should include a clear description, test commands run, and any linked issues.
- Add screenshots only when output is user-facing (CLI logs are fine as text).

## Security & Configuration Tips
- Use Python 3 for build scripts.
- For IDE support, generate compile commands in `cdk-c/` as needed (see `doc/cdk-dev/dev.md`).
