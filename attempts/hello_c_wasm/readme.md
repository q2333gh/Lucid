
clang folder is for wasm-wasi runtime usecase   example 
 
emcc is for browser wasm runtime usecase code example

**用途**: 用于将 C/C++ 代码编译为 WebAssembly，主要面向 Web 浏览器环境

## 🔍 两者的区别

| 特性 | WASI SDK | Emscripten SDK |
|------|----------|----------------|
| **目标环境** | WASI 运行时 (wasmtime, wasmer) | Web 浏览器 |
| **系统接口** | WASI 标准接口 | Web APIs + 自定义接口 |
| **文件大小** | 相对较小 | 较大（包含更多 Web 功能） |
| **使用场景** | 服务器端、命令行工具 | 前端 Web 应用 |
| **JavaScript 绑定** | 不生成 | 自动生成 JS 胶水代码 |


2. **创建安装脚本**:
   ```bash
   #!/bin/bash
   # install-sdks.sh
   
   # Install WASI SDK
   if [ ! -d "wasi-sdk-21.0" ]; then
     wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-21/wasi-sdk-21.0-linux.tar.gz
     tar xvf wasi-sdk-21.0-linux.tar.gz
     rm wasi-sdk-21.0-linux.tar.gz
   fi
   
   # Install Emscripten SDK
   if [ ! -d "emsdk" ]; then
     git clone https://github.com/emscripten-core/emsdk.git
     cd emsdk
     ./emsdk install latest
     ./emsdk activate latest
   fi
   ```

这样其他开发者可以通过运行脚本自动安装所需的 SDK，而不需要在版本控制中包含这些大型二进制文件。
