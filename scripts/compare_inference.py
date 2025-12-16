#!/usr/bin/env python3
"""
Compare Python (PyTorch) and C++ (ONNX) inference outputs
"""
import torch
import torch.nn as nn
from transformers import AutoTokenizer, AutoConfig, AutoModel, PreTrainedModel
import onnxruntime as ort
import numpy as np
import subprocess
import json
import sys

class DesklibAIDetectionModel(PreTrainedModel):
    config_class = AutoConfig

    def __init__(self, config):
        super().__init__(config)
        self.model = AutoModel.from_config(config)
        self.classifier = nn.Linear(config.hidden_size, 1)
        self.init_weights()

    def forward(self, input_ids, attention_mask=None, labels=None):
        outputs = self.model(input_ids, attention_mask=attention_mask)
        last_hidden_state = outputs[0]
        
        # Mean pooling
        input_mask_expanded = attention_mask.unsqueeze(-1).expand(last_hidden_state.size()).float()
        sum_embeddings = torch.sum(last_hidden_state * input_mask_expanded, dim=1)
        sum_mask = torch.clamp(input_mask_expanded.sum(dim=1), min=1e-9)
        pooled_output = sum_embeddings / sum_mask

        # Classifier
        logits = self.classifier(pooled_output)
        probability = torch.sigmoid(logits)
        
        return {"logits": logits, "probability": probability}


def test_python_model(text, model_dir, max_length=768):
    """Test with original PyTorch model"""
    print("=" * 60)
    print("PYTHON (PyTorch) INFERENCE")
    print("=" * 60)
    
    tokenizer = AutoTokenizer.from_pretrained(model_dir)
    model = DesklibAIDetectionModel.from_pretrained(model_dir)
    model.eval()
    
    encoded = tokenizer(
        text,
        padding='max_length',
        truncation=True,
        max_length=max_length,
        return_tensors='pt'
    )
    
    with torch.no_grad():
        outputs = model(
            input_ids=encoded['input_ids'],
            attention_mask=encoded['attention_mask']
        )
        probability = outputs["probability"].item()
    
    label = "AI-generated" if probability >= 0.5 else "Human-written"
    
    print(f"Text: \"{text[:80]}...\"")
    print(f"Probability: {probability:.6f}")
    print(f"Label: {label}")
    print()
    
    return probability, label


def test_onnx_model(text, onnx_path, tokenizer_path, max_length=768):
    """Test with ONNX Runtime"""
    print("=" * 60)
    print("ONNX RUNTIME INFERENCE")
    print("=" * 60)
    
    tokenizer = AutoTokenizer.from_pretrained(tokenizer_path)
    
    encoded = tokenizer(
        text,
        padding='max_length',
        truncation=True,
        max_length=max_length,
        return_tensors='np'
    )
    
    session = ort.InferenceSession(onnx_path)
    
    outputs = session.run(
        None,
        {
            'input_ids': encoded['input_ids'].astype(np.int64),
            'attention_mask': encoded['attention_mask'].astype(np.int64)
        }
    )
    
    probability = outputs[1][0][0]
    label = "AI-generated" if probability >= 0.5 else "Human-written"
    
    print(f"Text: \"{text[:80]}...\"")
    print(f"Probability: {probability:.6f}")
    print(f"Label: {label}")
    print()
    
    return probability, label


def test_cpp_inference(text, cpp_executable):
    """Test with C++ ONNX implementation (interactive mode)"""
    print("=" * 60)
    print("C++ (ONNX Runtime) INFERENCE")
    print("=" * 60)
    print(f"Running: {cpp_executable}")
    print(f"Text: \"{text[:80]}...\"")
    
    try:
        # Run the C++ test with --text flag for non-interactive mode
        result = subprocess.run(
            [cpp_executable, "--text", text],
            capture_output=True,
            text=True,
            timeout=30  # Increase timeout for model loading
        )
        
        output = result.stdout
        print(output)
        
        # Parse output to extract probability
        for line in output.split('\n'):
            if 'Probability:' in line:
                prob_str = line.split(':')[1].strip()
                probability = float(prob_str)
            if 'Label:' in line:
                label = line.split(':')[1].strip()
                return probability, label
        
        print("⚠️  Could not parse C++ output")
        return None, None
        
    except subprocess.TimeoutExpired:
        print("❌ C++ test timed out")
        return None, None
    except Exception as e:
        print(f"❌ C++ test failed: {e}")
        return None, None


def compare_results(test_cases, model_dir, onnx_path, cpp_executable=None):
    """Compare all three implementations"""
    print("\n" + "=" * 60)
    print("COMPARISON RESULTS")
    print("=" * 60)
    print()
    
    results = []
    
    for i, text in enumerate(test_cases, 1):
        print(f"\n{'#' * 60}")
        print(f"TEST CASE {i}")
        print(f"{'#' * 60}\n")
        
        # Python (PyTorch)
        py_prob, py_label = test_python_model(text, model_dir)
        
        # ONNX (Python)
        onnx_prob, onnx_label = test_onnx_model(text, onnx_path, model_dir)
        
        # C++ (if available)
        cpp_prob, cpp_label = None, None
        if cpp_executable:
            cpp_prob, cpp_label = test_cpp_inference(text, cpp_executable)
        
        # Calculate differences
        py_onnx_diff = abs(py_prob - onnx_prob) if onnx_prob else None
        py_cpp_diff = abs(py_prob - cpp_prob) if cpp_prob else None
        
        results.append({
            'text': text[:50] + "...",
            'pytorch': {'prob': py_prob, 'label': py_label},
            'onnx_py': {'prob': onnx_prob, 'label': onnx_label},
            'onnx_cpp': {'prob': cpp_prob, 'label': cpp_label},
            'py_onnx_diff': py_onnx_diff,
            'py_cpp_diff': py_cpp_diff
        })
    
    # Summary
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)
    print()
    
    print(f"{'Test':<10} {'PyTorch':<15} {'ONNX(Py)':<15} {'ONNX(C++)':<15} {'Py-ONNX Δ':<12} {'Py-C++ Δ':<12}")
    print("-" * 90)
    
    for i, r in enumerate(results, 1):
        pytorch_str = f"{r['pytorch']['prob']:.4f}"
        onnx_py_str = f"{r['onnx_py']['prob']:.4f}"
        onnx_cpp_str = f"{r['onnx_cpp']['prob']:.4f}" if r['onnx_cpp']['prob'] else "N/A"
        py_onnx_str = f"{r['py_onnx_diff']:.6f}" if r['py_onnx_diff'] else "N/A"
        py_cpp_str = f"{r['py_cpp_diff']:.6f}" if r['py_cpp_diff'] else "N/A"
        
        print(f"Case {i:<5} {pytorch_str:<15} {onnx_py_str:<15} {onnx_cpp_str:<15} {py_onnx_str:<12} {py_cpp_str:<12}")
    
    print()
    
    # Check if results match
    max_py_onnx_diff = max([r['py_onnx_diff'] for r in results if r['py_onnx_diff']])
    
    if max_py_onnx_diff < 0.001:
        print("✅ PASS: PyTorch and ONNX outputs match (difference < 0.001)")
    else:
        print(f"⚠️  WARNING: PyTorch and ONNX differ by up to {max_py_onnx_diff:.6f}")
    
    if any(r['py_cpp_diff'] for r in results):
        max_py_cpp_diff = max([r['py_cpp_diff'] for r in results if r['py_cpp_diff']])
        if max_py_cpp_diff < 0.001:
            print("✅ PASS: PyTorch and C++ ONNX outputs match (difference < 0.001)")
        else:
            print(f"⚠️  WARNING: PyTorch and C++ differ by up to {max_py_cpp_diff:.6f}")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Compare PyTorch and ONNX inference")
    parser.add_argument("--model-dir", default="desklib/ai-text-detector-v1.01",
                        help="Model directory or HuggingFace ID")
    parser.add_argument("--onnx-path", 
                        default="~/.local/share/ModAI/ModAI/data/models/ai_detector.onnx",
                        help="Path to ONNX model")
    parser.add_argument("--cpp-exe",
                        default="./build/test_onnx_inference",
                        help="Path to C++ test executable")
    parser.add_argument("--text", help="Single text to test")
    
    args = parser.parse_args()
    
    # Expand home directory
    import os
    onnx_path = os.path.expanduser(args.onnx_path)
    
    # Check if C++ executable exists
    cpp_exe = None
    if os.path.exists(args.cpp_exe):
        cpp_exe = args.cpp_exe
    else:
        print(f"⚠️  C++ executable not found: {args.cpp_exe}")
        print("   Skipping C++ comparison")
    
    # Test cases
    if args.text:
        test_cases = [args.text]
    else:
        test_cases = [
            "AI detection refers to the process of identifying whether a given piece of content, such as text, images, or audio, has been generated by artificial intelligence. This is achieved using various machine learning techniques, including perplexity analysis, entropy measurements, linguistic pattern recognition, and neural network classifiers trained on human and AI-generated data.",
            "It is estimated that a major part of the content in the internet will be generated by AI / LLMs by 2025. This leads to a lot of misinformation and credibility related issues. That is why if is important to have accurate tools to identify if a content is AI generated or human written.",
            "The quick brown fox jumps over the lazy dog. This is a simple test sentence."
        ]
    
    compare_results(test_cases, args.model_dir, onnx_path, cpp_exe)


if __name__ == "__main__":
    main()
