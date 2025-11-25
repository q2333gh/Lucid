# Development Guide

## IDE Support

Generate `compile_commands.json` for IDE code completion and navigation (clangd, VS Code, etc.):

```bash
cd cdk-c
bear python scripts/build.py --wasi --examples ( i use bear 2.4.4)
```

This will create `compile_commands.json` in the project root, enabling better IDE support for C code navigation and autocomplete.

### Language Server

We recommend using **clangd 21** (or compatible version) as the language server for the best IDE experience:

- **clangd 21.1+**: Recommended for optimal compatibility and feature support
- **clang 21.1+**: Should match clangd version for best results

The project is configured to work with clangd 21. If you encounter issues with `-std=c11` or other compiler flags, ensure your clangd version matches your clang version.

**Installation**: See [Clang 21 Installation Guide](clang21-install.md) for quick setup instructions.

## Development Workflow

1. **Build the project** (see [Build Guide](build.md))
2. **Generate compile commands** for IDE support (command above)
3. **Run tests** (see [Test Guide](test.md))
4. **Make changes** and rebuild as needed

## Tips

- The `compile_commands.json` file is git-ignored, so regenerate it after pulling changes or modifying build configuration
- Use `--clean` flag before regenerating compile commands if you encounter stale entries