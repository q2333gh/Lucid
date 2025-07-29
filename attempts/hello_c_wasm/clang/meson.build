project('wasm-add-function', 'c',
  version: '1.0.0',
  license: 'MIT',
  default_options: [
    'warning_level=2',
    'werror=false'
  ]
)

# Configuration options
build_minimal = get_option('minimal')
wasi_sdk_path = get_option('wasi_sdk_path')

# Auto-detect WASI SDK path if not provided
if wasi_sdk_path == ''
  wasi_sdk_path = meson.current_source_dir() / 'lib' / 'wasi-sdk-21.0'
endif

# Set up compiler
wasi_clang = wasi_sdk_path / 'bin' / 'clang'

# Check if WASI SDK exists
if not import('fs').exists(wasi_clang)
  error('WASI SDK not found at: ' + wasi_clang + '\nPlease set wasi_sdk_path option or ensure WASI SDK is installed.')
endif

# Override the C compiler to use WASI clang
cc = find_program(wasi_clang)

# Include directories
inc_dir = include_directories('include')

# Source files
add_sources = files('src/add.c')

# Common compiler arguments
common_args = ['-I' + meson.current_source_dir() / 'include']

# Target-specific configuration
if build_minimal
  # Minimal WASM without WASI runtime
  target_name = 'add_minimal'
  wasm_file = target_name + '.wasm'
  
  common_args += [
    '--target=wasm32',
    '-nostdlib',
    '-ffreestanding',
    '-O2'
  ]
  
  link_args = [
    '-Wl,--no-entry',
    '-Wl,--export=add',
    '-Wl,--allow-undefined'
  ]
else
  # Full WASI version
  target_name = 'add'
  wasm_file = target_name + '.wasm'
  
  common_args += [
    '--target=wasm32-wasi'
  ]
  
  link_args = [
    '-Wl,--export-all',
    '-Wl,--no-entry'
  ]
endif

# Create the WASM executable using custom command instead of executable()
# This gives us full control over the compilation process
wasm_obj = custom_target(target_name + '_obj',
  input: add_sources,
  output: target_name + '.o',
  command: [
    cc,
    common_args,
    '-c', '@INPUT@',
    '-o', '@OUTPUT@'
  ]
)

wasm_exe = custom_target(target_name + '_exe',
  input: wasm_obj,
  output: target_name,
  command: [
    cc,
    common_args,
    link_args,
    '@INPUT@',
    '-o', '@OUTPUT@'
  ]
)

# Custom target to rename output to .wasm extension
wasm_target = custom_target(wasm_file,
  input: wasm_exe,
  output: wasm_file,
  command: ['cp', '@INPUT@', '@OUTPUT@'],
  build_by_default: true
)

# Test runner
test_script = find_program('wasmtime', required: false)
if test_script.found()
  # Basic functionality tests
  test('add_5_3', test_script, 
    args: ['--invoke', 'add', wasm_target, '5', '3'],
    depends: wasm_target
  )
  
  test('add_10_20', test_script,
    args: ['--invoke', 'add', wasm_target, '10', '20'],
    depends: wasm_target
  )
  
  test('add_100_200', test_script,
    args: ['--invoke', 'add', wasm_target, '100', '200'],
    depends: wasm_target
  )
  
  test('add_negative', test_script,
    args: ['--invoke', 'add', wasm_target, '-5', '8'],
    depends: wasm_target
  )
else
  warning('wasmtime not found, tests will be skipped')
endif

# Custom targets for various operations
wasm2wat = find_program('wasm2wat', required: false)
if wasm2wat.found()
  # Generate WAT file
  wat_target = custom_target(target_name + '.wat',
    input: wasm_target,
    output: target_name + '.wat',
    command: [wasm2wat, '@INPUT@', '-o', '@OUTPUT@'],
    build_by_default: false
  )
  
  # Alias for generating WAT
  alias_target('wat', wat_target)
endif

# Summary
summary({
  'Build mode': build_minimal ? 'minimal' : 'full WASI',
  'WASI SDK path': wasi_sdk_path,
  'Target file': wasm_file,
  'Compiler': wasi_clang,
  'wasmtime available': test_script.found(),
  'wasm2wat available': wasm2wat.found()
}, section: 'Configuration')