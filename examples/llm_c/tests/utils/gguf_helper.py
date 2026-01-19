"""
GGUF Test Helper Utilities

Provides utilities for downloading and managing GGUF test models.
"""

import os
import hashlib
from pathlib import Path
from typing import Optional

# Model repository and file info
GGUF_MODEL_REPO = "mradermacher/tiny-gpt2-GGUF"
GGUF_MODEL_FILE = "tiny-gpt2-f16.gguf"
GGUF_MODEL_SIZE = 8_230_000  # Approximate size in bytes

def get_model_path() -> Path:
    """Get the path to the GGUF model file."""
    # Look in lib/ directory relative to test file
    test_dir = Path(__file__).parent.parent.parent
    lib_dir = test_dir / "lib"
    return lib_dir / GGUF_MODEL_FILE

def download_model(force: bool = False) -> Optional[Path]:
    """
    Download the GGUF model if it doesn't exist.
    
    Args:
        force: If True, re-download even if file exists
        
    Returns:
        Path to model file, or None on error
    """
    model_path = get_model_path()
    
    if model_path.exists() and not force:
        return model_path
    
    try:
        from huggingface_hub import hf_hub_download
        
        print(f"Downloading {GGUF_MODEL_FILE} from {GGUF_MODEL_REPO}...")
        downloaded_path = hf_hub_download(
            repo_id=GGUF_MODEL_REPO,
            filename=GGUF_MODEL_FILE,
            local_dir=str(model_path.parent),
            local_dir_use_symlinks=False
        )
        
        # Ensure it's in the expected location
        if downloaded_path != str(model_path):
            import shutil
            shutil.move(downloaded_path, str(model_path))
        
        print(f"Downloaded to {model_path}")
        return model_path
        
    except ImportError:
        print("Error: huggingface_hub not installed. Install with:")
        print("  pip install -U 'huggingface_hub[cli]'")
        return None
    except Exception as e:
        print(f"Error downloading model: {e}")
        return None

def verify_model_file(model_path: Optional[Path] = None) -> bool:
    """
    Verify that the model file exists and has reasonable size.
    
    Args:
        model_path: Path to model file (defaults to get_model_path())
        
    Returns:
        True if file is valid, False otherwise
    """
    if model_path is None:
        model_path = get_model_path()
    
    if not model_path.exists():
        return False
    
    # Check file size (should be around 8MB)
    size = model_path.stat().st_size
    if size < GGUF_MODEL_SIZE * 0.9 or size > GGUF_MODEL_SIZE * 1.1:
        print(f"Warning: Model file size {size} differs from expected ~{GGUF_MODEL_SIZE}")
        # Don't fail, just warn
    
    return True

def get_model_fixture():
    """
    Get model file path, downloading if necessary.
    
    Returns:
        Path to model file
        
    Raises:
        FileNotFoundError: If model cannot be obtained
    """
    model_path = get_model_path()
    
    if not verify_model_file(model_path):
        model_path = download_model()
        if not model_path or not verify_model_file(model_path):
            raise FileNotFoundError(
                f"GGUF model not found at {get_model_path()}. "
                f"Run: huggingface-cli download {GGUF_MODEL_REPO} --include '*f16*.gguf' --local-dir {get_model_path().parent}"
            )
    
    return model_path
