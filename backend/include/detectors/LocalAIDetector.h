#pragma once

#include "detectors/TextDetector.h"
#include <memory>
#include <string>
#include <vector>

namespace ModAI {

/**
 * Local AI text detector using ONNX Runtime
 * Uses the desklib/ai-text-detector-v1.01 model locally
 */
class LocalAIDetector : public TextDetector {
public:
    /**
     * @param modelPath Path to the ONNX model file
     * @param tokenizerPath Path to the tokenizer directory
     * @param maxLength Maximum sequence length (default: 768)
     * @param threshold Detection threshold (default: 0.5)
     */
    LocalAIDetector(const std::string& modelPath,
                    const std::string& tokenizerPath,
                    int maxLength = 768,
                    float threshold = 0.5f);
    
    ~LocalAIDetector() override;
    
    TextDetectResult analyze(const std::string& text) override;
    
    bool isAvailable() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    std::string modelPath_;
    std::string tokenizerPath_;
    int maxLength_;
    float threshold_;
    bool available_;
    
    // Tokenization
    struct TokenizedInput {
        std::vector<int64_t> input_ids;
        std::vector<int64_t> attention_mask;
    };
    
    TokenizedInput tokenize(const std::string& text);
    float runInference(const TokenizedInput& input);
};

} // namespace ModAI
