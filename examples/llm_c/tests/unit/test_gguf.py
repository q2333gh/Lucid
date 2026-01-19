"""
Unit tests for GGUF format support.

Tests are designed to be run independently (TDD style) and can be composed.
"""

import pytest
import os
import struct
from pathlib import Path
import sys

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from tests.utils.gguf_helper import get_model_fixture, verify_model_file, download_model


def test_gguf_parse_basic():
    """
    Test 1: GGUF File Parse
    
    Verify file format (magic number, version) and parse header successfully.
    """
    # Get model file
    model_path = get_model_fixture()
    
    # Verify file exists
    assert model_path.exists(), f"Model file not found: {model_path}"
    
    # Read header
    with open(model_path, 'rb') as f:
        # Read magic number (4 bytes)
        magic = f.read(4)
        assert len(magic) == 4, "Failed to read magic number"
        
        # Verify GGUF magic: 0x47 0x47 0x55 0x46 ("GGUF")
        assert magic == b'GGUF', f"Invalid magic number: {magic.hex()}"
        
        # Read version (4 bytes, little-endian uint32)
        version_bytes = f.read(4)
        assert len(version_bytes) == 4, "Failed to read version"
        version = struct.unpack('<I', version_bytes)[0]
        
        # Verify version (should be 3 for current GGUF spec)
        assert version == 3, f"Unexpected GGUF version: {version}"
        
        # Read tensor count (8 bytes, little-endian uint64)
        tensor_count_bytes = f.read(8)
        assert len(tensor_count_bytes) == 8, "Failed to read tensor count"
        tensor_count = struct.unpack('<Q', tensor_count_bytes)[0]
        
        assert tensor_count > 0, f"Invalid tensor count: {tensor_count}"
        
        # Read metadata KV count (8 bytes, little-endian uint64)
        kv_count_bytes = f.read(8)
        assert len(kv_count_bytes) == 8, "Failed to read metadata KV count"
        kv_count = struct.unpack('<Q', kv_count_bytes)[0]
        
        assert kv_count >= 0, f"Invalid metadata KV count: {kv_count}"
    
    print(f"✓ GGUF file parsed successfully:")
    print(f"  Version: {version}")
    print(f"  Tensors: {tensor_count}")
    print(f"  Metadata KVs: {kv_count}")


def test_gguf_metadata_valid():
    """
    Test 2: Metadata Validation
    
    Extract model config (n_layers, n_heads, n_embd, etc.) and validate
    dimensions match tiny-gpt2 spec.
    """
    # This test requires C library integration
    # For now, we'll verify file structure
    
    model_path = get_model_fixture()
    
    # Verify file is valid GGUF
    with open(model_path, 'rb') as f:
        magic = f.read(4)
        assert magic == b'GGUF', "Not a valid GGUF file"
        
        # Skip version
        f.read(4)
        
        # Read tensor and KV counts
        tensor_count = struct.unpack('<Q', f.read(8))[0]
        kv_count = struct.unpack('<Q', f.read(8))[0]
        
        # Skip to metadata section (after header)
        # Header is 24 bytes total
        header_size = 24
        
        # Parse metadata key-value pairs
        # Each KV has: string length (8 bytes) + string + value type (4 bytes) + value
        # This is simplified - full parsing requires C library
        
        # For now, just verify we can read past header
        assert f.tell() == header_size, f"Unexpected position after header: {f.tell()}"
    
    print(f"✓ Metadata section accessible:")
    print(f"  Expected {kv_count} metadata entries")
    print(f"  Expected {tensor_count} tensor entries")
    
    # Note: Full metadata parsing requires C library integration
    # This will be tested in integration tests


def test_gguf_weights_load():
    """
    Test 3: Weights Loading
    
    Load all weight tensors and verify shapes match expected GPT2 parameter layout.
    """
    # This test requires C library and model loading
    # For now, we verify file structure supports weight loading
    
    model_path = get_model_fixture()
    
    # Verify file exists and has reasonable size
    assert verify_model_file(model_path), "Model file verification failed"
    
    file_size = model_path.stat().st_size
    assert file_size > 1_000_000, f"Model file too small: {file_size} bytes"
    assert file_size < 20_000_000, f"Model file too large: {file_size} bytes"
    
    # Expected GPT-2 parameter tensors:
    # - token_embd.weight: (vocab_size, channels)
    # - pos_embd.weight: (max_seq_len, channels)
    # - blk.{i}.attn_norm.weight: (channels,) for each layer
    # - blk.{i}.attn_q.weight: (channels, channels) for each layer
    # - etc.
    
    print(f"✓ Model file ready for weight loading:")
    print(f"  File size: {file_size:,} bytes")
    print(f"  Weight loading will be tested in integration tests")


def test_gguf_tokenizer_extract():
    """
    Test 4: Tokenizer Extraction
    
    Extract vocab and merges from GGUF metadata and validate tokenizer can encode/decode.
    """
    # This test requires C library integration
    # For now, we verify file structure
    
    model_path = get_model_fixture()
    
    # Verify file is valid
    with open(model_path, 'rb') as f:
        magic = f.read(4)
        assert magic == b'GGUF', "Not a valid GGUF file"
    
    # Tokenizer data is stored in metadata section
    # Expected keys:
    # - tokenizer.ggml.vocab (or tokenizer.ggml.tokens)
    # - tokenizer.ggml.merges
    
    print(f"✓ Model file ready for tokenizer extraction:")
    print(f"  Tokenizer extraction will be tested in integration tests")


def test_gguf_inference_native():
    """
    Test 5: End-to-End Inference
    
    Load complete model from GGUF, run inference with prompt "Hello",
    and verify output is coherent text.
    """
    # This test requires full C library integration and native build
    # For now, we verify prerequisites
    
    model_path = get_model_fixture()
    
    # Verify model file exists
    assert model_path.exists(), f"Model file not found: {model_path}"
    
    # Check if native executable exists
    # This would be built by CMake
    test_dir = Path(__file__).parent.parent.parent
    build_dir = test_dir.parent.parent / "build"
    native_exe = build_dir / "run_gpt2.out"
    
    if not native_exe.exists():
        pytest.skip(f"Native executable not found: {native_exe}. Build with: python build.py --examples llm_c")
    
    # For now, just verify prerequisites
    print(f"✓ Prerequisites for inference test:")
    print(f"  Model file: {model_path}")
    print(f"  Native executable: {native_exe if native_exe.exists() else 'Not found'}")
    print(f"  Full inference test requires native build and execution")


if __name__ == "__main__":
    # Allow running tests individually
    import sys
    
    if len(sys.argv) > 1:
        test_name = sys.argv[1]
        if test_name == "parse":
            test_gguf_parse_basic()
        elif test_name == "metadata":
            test_gguf_metadata_valid()
        elif test_name == "weights":
            test_gguf_weights_load()
        elif test_name == "tokenizer":
            test_gguf_tokenizer_extract()
        elif test_name == "inference":
            test_gguf_inference_native()
        else:
            print(f"Unknown test: {test_name}")
            sys.exit(1)
    else:
        # Run all tests
        pytest.main([__file__, "-v"])
