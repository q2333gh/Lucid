# WASM Add Function - Meson Build System

这个项目现在支持使用 Meson 构建系统来编译 WebAssembly 模块。Meson 提供了更现代化和灵活的构建配置。

## 前提条件

1. **Meson** - 现代构建系统
   ```bash
   pip install meson
   # 或者
   apt install meson  # Ubuntu/Debian
   ```

2. **Ninja** - 快速构建后端（推荐）
   ```bash
   pip install ninja
   # 或者
   apt install ninja-build  # Ubuntu/Debian
   ```

3. **WASI SDK** - 必须已经安装在 `wasi-sdk-21.0/` 目录下

4. **wasmtime** - 用于测试 WASM 模块
   ```bash
   curl https://wasmtime.dev/install.sh -sSf | bash
   ```

## 快速开始

### 使用便捷脚本（推荐）

我们提供了一个便捷的构建脚本 `build.sh`：

```bash
# 构建完整的 WASI 版本
./build.sh

# 构建最小化版本
./build.sh --minimal

# 构建并运行测试
./build.sh --minimal --test

# 构建、测试并生成 WAT 文件
./build.sh --minimal --test --wat

# 查看所有选项
./build.sh --help

# 清理构建目录
./build.sh --clean
```

### 手动使用 Meson

#### 1. 构建完整的 WASI 版本

```bash
# 设置构建目录
meson setup build_full

# 编译
meson compile -C build_full

# 运行测试
meson test -C build_full

# 生成 WAT 文件
meson compile -C build_full wat
```

#### 2. 构建最小化版本

```bash
# 设置构建目录（启用最小化选项）
meson setup build_minimal -Dminimal=true

# 编译
meson compile -C build_minimal

# 运行测试
meson test -C build_minimal

# 生成 WAT 文件
meson compile -C build_minimal wat
```

## 构建选项

Meson 构建系统支持以下选项：

- `minimal` (boolean, 默认: false)
  - `true`: 构建最小化 WASM 模块，不包含 WASI 运行时
  - `false`: 构建完整的 WASI 版本

- `wasi_sdk_path` (string, 默认: 自动检测)
  - 指定 WASI SDK 的安装路径
  - 如果为空，会自动检测当前目录下的 `wasi-sdk-21.0/`

### 设置选项示例

```bash
# 构建最小化版本，指定 WASI SDK 路径
meson setup build_custom -Dminimal=true -Dwasi_sdk_path=/path/to/wasi-sdk

# 重新配置现有构建
meson configure build_full -Dminimal=true
```

## 输出文件

### 完整 WASI 版本
- 构建目录: `build_full/`
- WASM 文件: `build_full/add.wasm`
- WAT 文件: `build_full/add.wat` (可选)

### 最小化版本
- 构建目录: `build_minimal/`
- WASM 文件: `build_minimal/add_minimal.wasm`
- WAT 文件: `build_minimal/add_minimal.wat` (可选)

## 测试

Meson 内置了自动化测试：

```bash
# 运行所有测试
meson test -C build_minimal

# 运行特定测试
meson test -C build_minimal add_5_3

# 详细输出
meson test -C build_minimal --verbose
```

测试包括：
- `add_5_3`: 测试 add(5, 3) = 8
- `add_10_20`: 测试 add(10, 20) = 30
- `add_100_200`: 测试 add(100, 200) = 300
- `add_negative`: 测试 add(-5, 8) = 3

## 文件大小对比

使用最小化构建可以显著减少文件大小：

```bash
# 构建两个版本
./build.sh --full
./build.sh --minimal

# 比较大小
ls -lh build_full/add.wasm build_minimal/add_minimal.wasm

# 比较 WAT 行数
wc -l build_full/add.wat build_minimal/add_minimal.wat
```

预期结果：
- 完整 WASI 版本: ~21KB, ~7000 行 WAT
- 最小化版本: ~200B, ~20 行 WAT

## 清理

```bash
# 使用脚本清理
./build.sh --clean

# 手动清理
rm -rf build_full build_minimal
```

## Meson vs Makefile

| 特性 | Meson | Makefile |
|------|-------|----------|
| 配置管理 | ✅ 内置选项系统 | ❌ 手动变量 |
| 并行构建 | ✅ 自动并行 | ⚠️ 需要 -j 参数 |
| 依赖管理 | ✅ 自动依赖检测 | ❌ 手动管理 |
| 测试集成 | ✅ 内置测试框架 | ❌ 自定义脚本 |
| 交叉编译 | ✅ 原生支持 | ⚠️ 复杂配置 |
| IDE 集成 | ✅ 多种 IDE 支持 | ⚠️ 有限支持 |

## 故障排除

### WASI SDK 未找到
```
Error: WASI SDK not found at: wasi-sdk-21.0/bin/clang
```
解决方案：
- 确保 WASI SDK 已安装在正确位置
- 或使用 `-Dwasi_sdk_path` 选项指定路径

### wasmtime 未找到
```
WARNING: wasmtime not found, tests will be skipped
```
解决方案：
- 安装 wasmtime: `curl https://wasmtime.dev/install.sh -sSf | bash`
- 确保 wasmtime 在 PATH 中

### 权限错误
```
Permission denied: ./build.sh
```
解决方案：
```bash
chmod +x build.sh
```

## 高级用法

### 自定义编译器标志

编辑 `meson.build` 文件来添加自定义编译选项：

```meson
# 添加自定义 C 参数
common_args += ['-Wall', '-Wextra']
```

### 添加新的测试

在 `meson.build` 中添加新测试：

```meson
test('add_zero', test_script,
  args: ['--invoke', 'add', wasm_target, '0', '0'],
  depends: wasm_target
)
```

### 使用交叉编译文件

```bash
# 使用 WASI 交叉编译配置
meson setup build_cross --cross-file cross/wasi.ini

# 使用纯 WASM32 配置
meson setup build_wasm32 --cross-file cross/wasm32.ini
```