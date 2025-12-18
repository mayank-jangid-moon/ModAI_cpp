# Local AI Detection Setup Guide

This guide explains how to set up local AI text detection using the desklib/ai-text-detector-v1.01 model with ONNX Runtime.

## Overview

The application now supports **local inference** using ONNX Runtime, which means:
- No API calls or internet required for AI detection
- Faster inference (runs on your CPU/GPU)
- No API rate limits or costs
- Privacy - data stays on your machine

## Prerequisites

### 1. Install ONNX Runtime

#### Linux (Ubuntu/Debian)
```bash
# Download ONNX Runtime
wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-linux-x64-1.16.3.tgz
tar -xzf onnxruntime-linux-x64-1.16.3.tgz

# Install headers and library
sudo cp -r onnxruntime-linux-x64-1.16.3/include/* /usr/local/include/
sudo cp onnxruntime-linux-x64-1.16.3/lib/* /usr/local/lib/
sudo ldconfig
```

#### macOS
```bash
# Using Homebrew
brew install onnxruntime

# Or manually:
wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-osx-arm64-1.16.3.tgz
tar -xzf onnxruntime-osx-arm64-1.16.3.tgz
sudo cp -r onnxruntime-osx-arm64-1.16.3/include/* /usr/local/include/
sudo cp onnxruntime-osx-arm64-1.16.3/lib/* /usr/local/lib/
```

### 2. Install Python Dependencies (for model export)

```bash
pip install torch transformers onnx onnxruntime
```

## Export the Model to ONNX

### Step 1: Run the Export Script

```bash
cd /path/to/ModAI_cpp
python3 scripts/export_model_to_onnx.py --output ./models
```

This will:
1. Download the `desklib/ai-text-detector-v1.01` model from Hugging Face (~1.5GB)
2. Export it to ONNX format
3. Save tokenizer files
4. Create `model_config.json`

Expected output:
```
Loading model from desklib/ai-text-detector-v1.01...
Tokenizer saved to ./models
Exporting model to ONNX format...
Model exported to ./models/ai_detector.onnx
Configuration saved to ./models/model_config.json
✓ ONNX model verification successful!
  Output probability: 0.8523

✓ Export complete! Files saved to ./models/
  - ai_detector.onnx (model)
  - model_config.json (configuration)
  - tokenizer files (vocab.txt, tokenizer_config.json, etc.)
```

### Step 2: Copy Model to Application Data Directory

```bash
# Create models directory
mkdir -p ~/.local/share/ModAI/ModAI/data/models

# Copy exported model and tokenizer
cp -r models/* ~/.local/share/ModAI/ModAI/data/models/
```

Or you can specify the output directory directly:
```bash
python3 scripts/export_model_to_onnx.py \
    --output ~/.local/share/ModAI/ModAI/data/models
```

## Build the Application

### Rebuild with ONNX Support

```bash
cd build
cmake ..
cmake --build . -j$(nproc)
```

You should see:
```
-- Found ONNX Runtime: /usr/local/lib/libonnxruntime.so
-- Local AI detection enabled with ONNX Runtime
```

If ONNX Runtime is not found:
```
-- ONNX Runtime not found - local AI detection disabled
-- Install ONNX Runtime for local inference support
```

## Verify Setup

### Check Model Files

```bash
ls -lh ~/.local/share/ModAI/ModAI/data/models/
```

Expected files:
- `ai_detector.onnx` (1.3-1.5 GB)
- `model_config.json`
- `vocab.txt`
- `tokenizer_config.json`
- Other tokenizer files

### Run the Application

```bash
./build/ModAI
```

Check the status bar at the bottom of the window:
- "**✓ Local AI detection enabled**" - Local ONNX model is working
- ⚠ "**Using HuggingFace API for AI detection**" - Falling back to API (model not found)
- ⚠ "**AI detection disabled**" - No local model and no API key

Check the logs:
```bash
tail -f ~/.local/share/ModAI/ModAI/data/logs/system.log
```

Look for:
```
[INFO] Local AI Detector initialized successfully with ONNX Runtime
[INFO] Model: /path/to/ai_detector.onnx
[INFO] Using local ONNX AI detector (desklib/ai-text-detector-v1.01)
```

## Performance

### Inference Speed

- **CPU**: ~100-200ms per text (depending on length and CPU)
- **GPU**: Not yet supported (CPU only for now)

### Model Size

- Model file: ~1.3-1.5 GB
- RAM usage: ~2-3 GB during inference

### Accuracy

The model achieves top performance on the RAID benchmark:
- Visit: [RAID Leaderboard](https://raid-bench.xyz)
- Model: `desklib/ai-text-detector-v1.01`

## Troubleshooting

### ONNX Runtime Not Found

**Error**: `-- ONNX Runtime not found`

**Solution**:
1. Install ONNX Runtime (see Prerequisites above)
2. Make sure `libonnxruntime.so` (Linux) or `libonnxruntime.dylib` (macOS) is in `/usr/local/lib/`
3. Run `sudo ldconfig` (Linux)

### Model Export Fails

**Error**: `Failed to download model` or `Out of memory`

**Solution**:
1. Make sure you have at least 4GB free RAM
2. Check your internet connection
3. Try with a smaller model first (for testing)

### Tokenization Issues

**Error**: `Failed to load vocabulary`

**Solution**:
1. Make sure `vocab.txt` exists in the models directory
2. Re-export the model
3. Check file permissions

### Runtime Errors

**Error**: `ONNX inference error` or `Session creation failed`

**Solution**:
1. Verify model file is not corrupted: `ls -lh ai_detector.onnx`
2. Re-export the model
3. Check ONNX Runtime version compatibility

## Fallback Behavior

The application has smart fallback logic:

1. **Try local ONNX model first** (if available)
2. **Fall back to HuggingFace API** (if API key is set)
3. **Disable AI detection** (if neither is available)

This means you can run the app even without the local model - it will just use the API instead.

## Advanced Options

### Custom Model Path

You can modify the model path in the code:
```cpp
// In MainWindow.cpp
std::string modelPath = "/custom/path/to/ai_detector.onnx";
std::string tokenizerPath = "/custom/path/to/tokenizer";
```

### Adjust Detection Threshold

```cpp
// In MainWindow.cpp (default is 0.5)
auto localDetector = std::make_unique<LocalAIDetector>(
    modelPath, tokenizerPath, 768, 0.7f  // Higher threshold = more conservative
);
```

### Maximum Sequence Length

```cpp
// In MainWindow.cpp (default is 768)
auto localDetector = std::make_unique<LocalAIDetector>(
    modelPath, tokenizerPath, 512, 0.5f  // Shorter = faster but may cut text
);
```

## Benefits of Local Inference

| Feature | Local ONNX | HuggingFace API |
|---------|------------|-----------------|
| Speed | Fast (100-200ms) | Slow (1-3s) |
| Privacy | Fully private | Sends to cloud |
| Cost | Free | Rate limits |
| Internet | Not required | Required |
| Setup | One-time setup | Just API key |

## References

- **Model**: [desklib/ai-text-detector-v1.01](https://huggingface.co/desklib/ai-text-detector-v1.01)
- **ONNX Runtime**: [microsoft/onnxruntime](https://github.com/microsoft/onnxruntime)
- **RAID Benchmark**: [raid-bench.xyz](https://raid-bench.xyz)
- **Desklib AI Detector**: [try online](https://www.desklib.com/ai-detector)

## Support

If you encounter issues:
1. Check the application logs
2. Verify all files are in place
3. Rebuild with `cmake --build . --clean-first`
4. Open an issue on GitHub with logs
