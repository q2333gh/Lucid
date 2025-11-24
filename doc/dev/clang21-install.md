# Clang 21 Installation Guide

## Ubuntu/Debian

```bash
# 1. Add LLVM official repository
sudo apt install -y software-properties-common wget gnupg2
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo add-apt-repository "deb https://apt.llvm.org/$(lsb_release -sc)/ llvm-toolchain-$(lsb_release -sc)-21 main"

# 2. Install clang 21 and clangd 21
sudo apt update
sudo apt install -y clang-21 clangd-21

# 3. Set as default version (optional)
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-21 100
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-21 100
sudo update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-21 100

# 4. Verify installation
clang --version
clangd --version
```

## Verification

After successful installation, you should see:

```
Ubuntu clang version 21.1.5
Ubuntu clangd version 21.1.5
```

## VS Code/Cursor Configuration

Add the following to `.vscode/settings.json`:

```json
{
  "clangd.path": "/usr/bin/clangd"
}
```

## References

- [LLVM APT Repository](https://apt.llvm.org/)
- [Clangd Documentation](https://clangd.llvm.org/)

