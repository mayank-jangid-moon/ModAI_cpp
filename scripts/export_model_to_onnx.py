 #!/usr/bin/env python3
"""
Export desklib/ai-text-detector-v1.01 model to ONNX format for local C++ inference
"""
import torch
import torch.nn as nn
from transformers import AutoTokenizer, AutoConfig, AutoModel, PreTrainedModel
import json
import os

class DesklibAIDetectionModel(PreTrainedModel):
    config_class = AutoConfig

    def __init__(self, config):
        super().__init__(config)
        # Initialize the base transformer model.
        self.model = AutoModel.from_config(config)
        # Define a classifier head.
        self.classifier = nn.Linear(config.hidden_size, 1)
        # Initialize weights (handled by PreTrainedModel)
        self.init_weights()

    def forward(self, input_ids, attention_mask=None, labels=None):
        # Forward pass through the transformer
        outputs = self.model(input_ids, attention_mask=attention_mask)
        last_hidden_state = outputs[0]
        # Mean pooling
        input_mask_expanded = attention_mask.unsqueeze(-1).expand(last_hidden_state.size()).float()
        sum_embeddings = torch.sum(last_hidden_state * input_mask_expanded, dim=1)
        sum_mask = torch.clamp(input_mask_expanded.sum(dim=1), min=1e-9)
        pooled_output = sum_embeddings / sum_mask

        # Classifier
        logits = self.classifier(pooled_output)
        
        # Return logits and probability
        probability = torch.sigmoid(logits)
        
        return {"logits": logits, "probability": probability}


def export_to_onnx(model_directory="desklib/ai-text-detector-v1.01", 
                   output_dir="./models",
                   max_length=768):
    """
    Export the model to ONNX format
    """
    import sys
    import time
    
    print(f"Loading model from {model_directory}...")
    sys.stdout.flush()
    
    start_time = time.time()
    
    # Load tokenizer and model
    tokenizer = AutoTokenizer.from_pretrained(model_directory)
    print(f"✓ Tokenizer loaded ({time.time() - start_time:.1f}s)")
    sys.stdout.flush()
    
    model = DesklibAIDetectionModel.from_pretrained(model_directory)
    model.eval()
    print(f"✓ Model loaded ({time.time() - start_time:.1f}s)")
    sys.stdout.flush()
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Save tokenizer
    print(f"\n[1/5] Saving tokenizer...")
    sys.stdout.flush()
    tokenizer.save_pretrained(output_dir)
    print(f"✓ Tokenizer saved to {output_dir} ({time.time() - start_time:.1f}s)")
    sys.stdout.flush()
    
    # Create dummy input for tracing
    print(f"\n[2/5] Preparing dummy inputs for export...")
    sys.stdout.flush()
    dummy_text = "This is a sample text for model export."
    encoded = tokenizer(
        dummy_text,
        padding='max_length',
        truncation=True,
        max_length=max_length,
        return_tensors='pt'
    )
    
    input_ids = encoded['input_ids']
    attention_mask = encoded['attention_mask']
    print(f"✓ Dummy inputs prepared ({time.time() - start_time:.1f}s)")
    sys.stdout.flush()
    
    # Export to ONNX
    onnx_path = os.path.join(output_dir, "ai_detector.onnx")
    
    print(f"\n[3/5] Exporting model to ONNX format...")
    print(f"⏳ This step takes 2-5 minutes (model size: ~1.5GB)")
    print(f"   Progress indicators:")
    sys.stdout.flush()
    
    # Start a simple progress indicator in a thread
    import threading
    stop_progress = threading.Event()
    
    def show_progress():
        chars = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏']
        i = 0
        while not stop_progress.is_set():
            sys.stdout.write(f'\r   {chars[i % len(chars)]} Exporting... ({int(time.time() - start_time)}s elapsed)')
            sys.stdout.flush()
            time.sleep(0.1)
            i += 1
        sys.stdout.write('\r')
        sys.stdout.flush()
    
    progress_thread = threading.Thread(target=show_progress)
    progress_thread.start()
    
    try:
        torch.onnx.export(
            model,
            (input_ids, attention_mask),
            onnx_path,
            export_params=True,
            opset_version=18,  # Use newer opset version (faster, more stable)
            do_constant_folding=True,
            input_names=['input_ids', 'attention_mask'],
            output_names=['logits', 'probability'],
            dynamic_axes={
                'input_ids': {0: 'batch_size', 1: 'sequence'},
                'attention_mask': {0: 'batch_size', 1: 'sequence'},
                'logits': {0: 'batch_size'},
                'probability': {0: 'batch_size'}
            },
            verbose=False,  # Reduce output noise
            dynamo=False  # Disable dynamo for stable export
        )
    finally:
        stop_progress.set()
        progress_thread.join()
    
    print(f"✓ Model exported to {onnx_path} ({time.time() - start_time:.1f}s)")
    sys.stdout.flush()
    
    # Save model configuration
    print(f"\n[4/5] Saving configuration...")
    sys.stdout.flush()
    config = {
        "max_length": max_length,
        "model_name": model_directory,
        "vocab_size": tokenizer.vocab_size,
        "hidden_size": model.config.hidden_size,
        "threshold": 0.5
    }
    
    config_path = os.path.join(output_dir, "model_config.json")
    with open(config_path, 'w') as f:
        json.dump(config, f, indent=2)
    
    print(f"✓ Configuration saved to {config_path} ({time.time() - start_time:.1f}s)")
    sys.stdout.flush()
    
    # Test the ONNX model
    print(f"\n[5/5] Verifying ONNX model...")
    sys.stdout.flush()
    import onnxruntime as ort
    
    session = ort.InferenceSession(onnx_path)
    
    # Run inference
    outputs = session.run(
        None,
        {
            'input_ids': input_ids.numpy(),
            'attention_mask': attention_mask.numpy()
        }
    )
    
    print(f"✓ ONNX model verification successful!")
    print(f"  Test output probability: {outputs[1][0][0]:.4f}")
    print(f"\n{'='*60}")
    print(f"✅ Export complete! Total time: {time.time() - start_time:.1f}s")
    print(f"{'='*60}")
    print(f"\nFiles saved to {output_dir}/:")
    print(f"  📦 ai_detector.onnx (model)")
    print(f"  📄 model_config.json (configuration)")
    print(f"  📝 tokenizer files (vocab.txt, tokenizer_config.json, etc.)")
    print(f"\nNext steps:")
    print(f"  1. Rebuild: cd build && cmake .. && cmake --build .")
    print(f"  2. Run: ./ModAI")


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Export Desklib AI Detector to ONNX")
    parser.add_argument("--model", default="desklib/ai-text-detector-v1.01",
                        help="Hugging Face model ID or local path")
    parser.add_argument("--output", default="./models",
                        help="Output directory for ONNX model and tokenizer")
    parser.add_argument("--max-length", type=int, default=768,
                        help="Maximum sequence length")
    
    args = parser.parse_args()
    
    export_to_onnx(args.model, args.output, args.max_length)
