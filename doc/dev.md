# Development Guide

## IDE Support

Generate `compile_commands.json` for IDE code completion and navigation (clangd, VS Code, etc.):

```bash
cd cdk-c
bear python scripts/build.py --wasi --examples
```

This will create `compile_commands.json` in the project root, enabling better IDE support for C code navigation and autocomplete.

## Development Workflow

1. **Build the project** (see [Build Guide](build.md))
2. **Generate compile commands** for IDE support (command above)
3. **Run tests** (see [Test Guide](test.md))
4. **Make changes** and rebuild as needed

## Tips

- The `compile_commands.json` file is git-ignored, so regenerate it after pulling changes or modifying build configuration
- Use `--clean` flag before regenerating compile commands if you encounter stale entries