# Local AI Detection with ONNX

This implementation adds **local inference** support using ONNX Runtime for the `desklib/ai-text-detector-v1.01` model.

## Quick Start

### 1. Install ONNX Runtime

```bash
./scripts/install_onnxruntime.sh
```

### 2. Export the Model

```bash
# Install Python dependencies
pip install torch transformers onnx onnxruntime

# Export model to ONNX format
python3 scripts/export_model_to_onnx.py \
    --output ~/.local/share/ModAI/ModAI/data/models
```

### 3. Rebuild and Run

```bash
cd build
cmake ..
cmake --build . -j$(nproc)
./ModAI
```

## What Changed

### New Files

- `include/detectors/LocalAIDetector.h` - Local ONNX detector interface
- `src/detectors/LocalAIDetector.cpp` - Implementation with tokenization
- `scripts/export_model_to_onnx.py` - Model export tool
- `scripts/install_onnxruntime.sh` - ONNX Runtime installer
- `docs/LOCAL_AI_SETUP.md` - Comprehensive setup guide

### Modified Files

- `CMakeLists.txt` - Added ONNX Runtime detection and linking
- `src/ui/MainWindow.cpp` - Smart fallback: local model → API → disabled

## Features

✅ **Local inference** - No API calls, runs on your machine  
✅ **Faster** - 100-200ms vs 1-3s for API calls  
✅ **Private** - Data never leaves your computer  
✅ **No rate limits** - Process unlimited texts  
✅ **Smart fallback** - Uses API if local model unavailable  
✅ **Easy setup** - One-time model export  

## Architecture

### Tokenization

Uses a simplified DeBERTa tokenizer implementation:
- Loads `vocab.txt` from Hugging Face
- WordPiece-style tokenization
- Max sequence length: 768 tokens
- Special tokens: [CLS], [SEP], [PAD], [UNK]

### ONNX Inference

```cpp
LocalAIDetector detector(modelPath, tokenizerPath, maxLength, threshold);
TextDetectResult result = detector.analyze("Sample text...");
// result.ai_score: 0.0-1.0 probability
// result.label: "ai" or "human"
```

### Model Format

The exported ONNX model has:
- **Inputs**: `input_ids` (int64), `attention_mask` (int64)
- **Outputs**: `probability` (float32)
- **Size**: ~1.3-1.5 GB
- **Precision**: FP32

## Performance

| Metric | Value |
|--------|-------|
| Inference time | 100-200ms (CPU) |
| Model size | 1.3-1.5 GB |
| RAM usage | 2-3 GB |
| Accuracy | Top RAID Benchmark |

## Troubleshooting

### Build fails with "ONNX Runtime not found"

Install ONNX Runtime:
```bash
./scripts/install_onnxruntime.sh
```

### Runtime error: "Failed to load vocabulary"

Re-export the model with tokenizer files:
```bash
python3 scripts/export_model_to_onnx.py --output models
```

### App shows "Using HuggingFace API"

Model not found. Check:
```bash
ls -lh ~/.local/share/ModAI/ModAI/data/models/ai_detector.onnx
```

If missing, run the export script.

## References

- [desklib/ai-text-detector-v1.01](https://huggingface.co/desklib/ai-text-detector-v1.01)
- [ONNX Runtime](https://github.com/microsoft/onnxruntime)
- [Complete Setup Guide](../docs/LOCAL_AI_SETUP.md)
