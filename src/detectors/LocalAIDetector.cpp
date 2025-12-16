#include "detectors/LocalAIDetector.h"
#include "utils/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>

#ifdef ONNXRUNTIME_FOUND
#include <onnxruntime_cxx_api.h>
#endif

namespace ModAI {

// Simple tokenizer implementation for DeBERTa
class SimpleTokenizer {
public:
    SimpleTokenizer(const std::string& vocabPath) {
        loadVocab(vocabPath);
    }
    
    std::vector<int64_t> encode(const std::string& text, int maxLength) {
        std::vector<int64_t> tokens;
        
        // Add [CLS] token
        tokens.push_back(getTokenId("[CLS]"));
        
        // SentencePiece-style tokenization (used by DeBERTa)
        std::istringstream iss(text);
        std::string word;
        
        while (iss >> word && tokens.size() < maxLength - 1) {
            // Split trailing punctuation from word
            while (!word.empty() && std::ispunct(word.back()) && tokens.size() < maxLength - 1) {
                char punct = word.back();
                word.pop_back();
                
                // Queue punctuation token for later
                if (!word.empty()) {
                    // Process word first
                    std::string spToken = "\u2581" + word;
                    auto it = vocab_.find(spToken);
                    if (it != vocab_.end()) {
                        tokens.push_back(it->second);
                    } else {
                        tokens.push_back(getTokenId("[UNK]"));
                    }
                    word.clear();
                }
                
                // Then add punctuation (without ▁ prefix for punctuation)
                std::string punctStr(1, punct);
                auto punct_it = vocab_.find(punctStr);
                if (punct_it != vocab_.end()) {
                    tokens.push_back(punct_it->second);
                } else {
                    // Try with ▁ prefix
                    std::string spPunct = "\u2581" + punctStr;
                    punct_it = vocab_.find(spPunct);
                    if (punct_it != vocab_.end()) {
                        tokens.push_back(punct_it->second);
                    } else {
                        tokens.push_back(getTokenId("[UNK]"));
                    }
                }
            }
            
            // Process remaining word (if any)
            if (!word.empty()) {
                std::string spToken = "\u2581" + word;
                auto it = vocab_.find(spToken);
                if (it != vocab_.end()) {
                    tokens.push_back(it->second);
                } else {
                    tokens.push_back(getTokenId("[UNK]"));
                }
            }
        }
        
        // Add [SEP] token
        tokens.push_back(getTokenId("[SEP]"));
        
        // Pad or truncate to maxLength
        if (tokens.size() < maxLength) {
            tokens.resize(maxLength, getTokenId("[PAD]"));
        } else if (tokens.size() > maxLength) {
            tokens.resize(maxLength);
            tokens[maxLength - 1] = getTokenId("[SEP]");
        }
        
        return tokens;
    }
    
    std::vector<int64_t> getAttentionMask(const std::vector<int64_t>& tokens) {
        std::vector<int64_t> mask;
        int64_t padId = getTokenId("[PAD]");
        
        for (auto token : tokens) {
            mask.push_back(token != padId ? 1 : 0);
        }
        
        return mask;
    }

private:
    std::unordered_map<std::string, int64_t> vocab_;
    
    void loadVocab(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            Logger::error("Failed to load vocabulary from: " + path);
            return;
        }
        
        std::string token;
        int64_t id = 0;
        
        while (std::getline(file, token)) {
            if (!token.empty()) {
                vocab_[token] = id++;
            }
        }
        
        Logger::info("Loaded vocabulary with " + std::to_string(vocab_.size()) + " tokens");
    }
    
    int64_t getTokenId(const std::string& token) {
        auto it = vocab_.find(token);
        if (it != vocab_.end()) {
            return it->second;
        }
        // Return [UNK] token id, typically 1
        auto unk_it = vocab_.find("[UNK]");
        return unk_it != vocab_.end() ? unk_it->second : 1;
    }
};

// PIMPL implementation
struct LocalAIDetector::Impl {
#ifdef ONNXRUNTIME_FOUND
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;
    Ort::SessionOptions sessionOptions;
    Ort::AllocatorWithDefaultOptions allocator;
#endif
    std::unique_ptr<SimpleTokenizer> tokenizer;
    
    Impl() 
#ifdef ONNXRUNTIME_FOUND
        : env(ORT_LOGGING_LEVEL_WARNING, "LocalAIDetector")
#endif
    {}
};

LocalAIDetector::LocalAIDetector(const std::string& modelPath,
                                 const std::string& tokenizerPath,
                                 int maxLength,
                                 float threshold)
    : impl_(std::make_unique<Impl>())
    , modelPath_(modelPath)
    , tokenizerPath_(tokenizerPath)
    , maxLength_(maxLength)
    , threshold_(threshold)
    , available_(false) {
    
#ifdef ONNXRUNTIME_FOUND
    try {
        // Load tokenizer
        std::string vocabPath = tokenizerPath_ + "/vocab.txt";
        impl_->tokenizer = std::make_unique<SimpleTokenizer>(vocabPath);
        
        // Initialize ONNX Runtime session
        impl_->sessionOptions.SetIntraOpNumThreads(1);
        impl_->sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        
        impl_->session = std::make_unique<Ort::Session>(
            impl_->env,
            modelPath_.c_str(),
            impl_->sessionOptions
        );
        
        available_ = true;
        Logger::info("Local AI Detector initialized successfully with ONNX Runtime");
        Logger::info("Model: " + modelPath_);
    } catch (const std::exception& e) {
        Logger::error("Failed to initialize Local AI Detector: " + std::string(e.what()));
        available_ = false;
    }
#else
    Logger::warn("ONNX Runtime not available - Local AI Detector disabled");
    Logger::warn("Please install ONNX Runtime to use local inference");
    available_ = false;
#endif
}

LocalAIDetector::~LocalAIDetector() = default;

bool LocalAIDetector::isAvailable() const {
    return available_;
}

LocalAIDetector::TokenizedInput LocalAIDetector::tokenize(const std::string& text) {
    TokenizedInput result;
    
    if (!impl_->tokenizer) {
        return result;
    }
    
    result.input_ids = impl_->tokenizer->encode(text, maxLength_);
    result.attention_mask = impl_->tokenizer->getAttentionMask(result.input_ids);
    
    return result;
}

float LocalAIDetector::runInference(const TokenizedInput& input) {
#ifdef ONNXRUNTIME_FOUND
    try {
        // Prepare input tensors
        std::vector<int64_t> inputShape = {1, static_cast<int64_t>(maxLength_)};
        
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        
        // Create input tensors
        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memoryInfo,
            const_cast<int64_t*>(input.input_ids.data()),
            input.input_ids.size(),
            inputShape.data(),
            inputShape.size()
        ));
        
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memoryInfo,
            const_cast<int64_t*>(input.attention_mask.data()),
            input.attention_mask.size(),
            inputShape.data(),
            inputShape.size()
        ));
        
        // Input/output names
        const char* inputNames[] = {"input_ids", "attention_mask"};
        const char* outputNames[] = {"probability"};
        
        // Run inference
        auto outputTensors = impl_->session->Run(
            Ort::RunOptions{nullptr},
            inputNames,
            inputTensors.data(),
            2,
            outputNames,
            1
        );
        
        // Get probability output
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        return outputData[0];
        
    } catch (const std::exception& e) {
        Logger::error("ONNX inference error: " + std::string(e.what()));
        return 0.0f;
    }
#else
    return 0.0f;
#endif
}

TextDetectResult LocalAIDetector::analyze(const std::string& text) {
    TextDetectResult result;
    result.label = "unknown";
    result.ai_score = 0.0;
    result.confidence = 0.0;
    
    if (!available_) {
        Logger::warn("Local AI Detector not available - skipping analysis");
        return result;
    }
    
    if (text.empty() || text.length() < 10) {
        result.label = "human";
        result.ai_score = 0.0;
        result.confidence = 1.0;
        return result;
    }
    
    try {
        // Tokenize input
        auto tokenized = tokenize(text);
        
        if (tokenized.input_ids.empty()) {
            Logger::error("Tokenization failed");
            return result;
        }
        
        // Run inference
        float probability = runInference(tokenized);
        
        // Set result
        result.ai_score = probability;
        result.confidence = std::abs(probability - 0.5f) * 2.0f; // Convert to 0-1 confidence
        result.label = probability >= threshold_ ? "ai" : "human";
        
        Logger::debug("AI Detection - Text length: " + std::to_string(text.length()) +
                     ", Probability: " + std::to_string(probability) +
                     ", Label: " + result.label);
        
    } catch (const std::exception& e) {
        Logger::error("Exception in Local AI Detector: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace ModAI
