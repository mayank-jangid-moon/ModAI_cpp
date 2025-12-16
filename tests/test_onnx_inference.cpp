#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <iomanip>

#ifdef ONNXRUNTIME_FOUND
#include <onnxruntime_cxx_api.h>
#endif

// Simple tokenizer for testing
class SimpleTokenizer {
public:
    SimpleTokenizer(const std::string& vocabPath) {
        loadVocab(vocabPath);
    }
    
    std::vector<int64_t> encode(const std::string& text, int maxLength) {
        std::vector<int64_t> tokens;
        tokens.push_back(getTokenId("[CLS]"));
        
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
        
        tokens.push_back(getTokenId("[SEP]"));
        
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
            std::cerr << "Error: Failed to load vocabulary from: " << path << std::endl;
            return;
        }
        
        std::string token;
        int64_t id = 0;
        
        while (std::getline(file, token)) {
            if (!token.empty()) {
                vocab_[token] = id++;
            }
        }
        
        std::cout << "✓ Loaded vocabulary with " << vocab_.size() << " tokens" << std::endl;
    }
    
    int64_t getTokenId(const std::string& token) {
        auto it = vocab_.find(token);
        if (it != vocab_.end()) {
            return it->second;
        }
        auto unk_it = vocab_.find("[UNK]");
        return unk_it != vocab_.end() ? unk_it->second : 1;
    }
};

#ifdef ONNXRUNTIME_FOUND
float runInference(Ort::Session& session, const std::vector<int64_t>& input_ids, 
                   const std::vector<int64_t>& attention_mask, int maxLength) {
    try {
        std::vector<int64_t> inputShape = {1, static_cast<int64_t>(maxLength)};
        
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        
        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memoryInfo,
            const_cast<int64_t*>(input_ids.data()),
            input_ids.size(),
            inputShape.data(),
            inputShape.size()
        ));
        
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memoryInfo,
            const_cast<int64_t*>(attention_mask.data()),
            attention_mask.size(),
            inputShape.data(),
            inputShape.size()
        ));
        
        const char* inputNames[] = {"input_ids", "attention_mask"};
        const char* outputNames[] = {"probability"};
        
        auto outputTensors = session.Run(
            Ort::RunOptions{nullptr},
            inputNames,
            inputTensors.data(),
            2,
            outputNames,
            1
        );
        
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        return outputData[0];
        
    } catch (const std::exception& e) {
        std::cerr << "Inference error: " << e.what() << std::endl;
        return -1.0f;
    }
}
#endif

int main(int argc, char* argv[]) {
    // Check for non-interactive mode (for automated testing)
    bool interactive = true;
    std::string testText;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--no-interactive" || arg == "-n") {
            interactive = false;
        } else if (arg == "--text" && i + 1 < argc) {
            testText = argv[++i];
            interactive = false;
        }
    }
    
    if (interactive) {
        std::cout << "========================================" << std::endl;
        std::cout << "ONNX AI Detector Test" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
    }
    
#ifndef ONNXRUNTIME_FOUND
    std::cerr << "❌ Error: ONNX Runtime not available!" << std::endl;
    std::cerr << "Please install ONNX Runtime and rebuild with:" << std::endl;
    std::cerr << "  ./scripts/install_onnxruntime.sh" << std::endl;
    std::cerr << "  cd build && cmake .. && cmake --build ." << std::endl;
    return 1;
#else
    
    // Default paths
    std::string modelPath = std::string(getenv("HOME")) + "/.local/share/ModAI/ModAI/data/models/ai_detector.onnx";
    std::string vocabPath = std::string(getenv("HOME")) + "/.local/share/ModAI/ModAI/data/models/vocab.txt";
    
    // Override with command line arguments if provided
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (arg == "--vocab" && i + 1 < argc) {
            vocabPath = argv[++i];
        }
    }
    
    if (interactive) {
        std::cout << "Model path: " << modelPath << std::endl;
        std::cout << "Vocab path: " << vocabPath << std::endl;
        std::cout << std::endl;
    }
    
    // Check if files exist
    std::ifstream modelFile(modelPath);
    std::ifstream vocabFile(vocabPath);
    
    if (!modelFile.good()) {
        std::cerr << "❌ Error: Model file not found!" << std::endl;
        std::cerr << "Expected: " << modelPath << std::endl;
        if (interactive) {
            std::cerr << std::endl;
            std::cerr << "Please export the model first:" << std::endl;
            std::cerr << "  python3 scripts/export_model_to_onnx.py --output ~/.local/share/ModAI/ModAI/data/models" << std::endl;
        }
        return 1;
    }
    
    if (!vocabFile.good()) {
        std::cerr << "❌ Error: Vocabulary file not found!" << std::endl;
        std::cerr << "Expected: " << vocabPath << std::endl;
        return 1;
    }
    
    if (interactive) std::cout << "[1/4] Loading tokenizer..." << std::endl;
    SimpleTokenizer tokenizer(vocabPath);
    if (interactive) std::cout << std::endl;
    
    if (interactive) std::cout << "[2/4] Loading ONNX model..." << std::endl;
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "ONNXTest");
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        
        Ort::Session session(env, modelPath.c_str(), sessionOptions);
        if (interactive) {
            std::cout << "✓ Model loaded successfully" << std::endl;
            std::cout << std::endl;
        }
        
        const int maxLength = 768;
        const float threshold = 0.5f;
        
        // If specific text provided, just test that
        if (!testText.empty()) {
            auto input_ids = tokenizer.encode(testText, maxLength);
            auto attention_mask = tokenizer.getAttentionMask(input_ids);
            float probability = runInference(session, input_ids, attention_mask, maxLength);
            
            if (probability >= 0) {
                std::string label = probability >= threshold ? "AI-generated" : "Human-written";
                std::cout << "Text: \"" << testText.substr(0, 80) << "...\"" << std::endl;
                std::cout << "Probability: " << std::fixed << std::setprecision(6) << probability << std::endl;
                std::cout << "Label: " << label << std::endl;
                return 0;
            } else {
                std::cerr << "❌ Inference failed" << std::endl;
                return 1;
            }
        }
        
        // Test texts (only run if interactive mode)
        if (!interactive) {
            return 0;
        }
        
        std::vector<std::pair<std::string, std::string>> testCases = {
            {"AI detection refers to the process of identifying whether a given piece of content has been generated by artificial intelligence. This is achieved using various machine learning techniques.", "AI-generated"},
            {"It is estimated that a major part of the content in the internet will be generated by AI by 2025. This leads to a lot of misinformation.", "Human-written"},
            {"The quick brown fox jumps over the lazy dog.", "Human-written"}
        };
        
        std::cout << "[3/4] Running test cases..." << std::endl;
        std::cout << std::endl;
        
        for (size_t i = 0; i < testCases.size(); i++) {
            const auto& text = testCases[i].first;
            const auto& expectedLabel = testCases[i].second;
            
            std::cout << "Test Case " << (i + 1) << ":" << std::endl;
            std::cout << "Text: \"" << text.substr(0, 80) << "...\"" << std::endl;
            
            // Tokenize
            auto input_ids = tokenizer.encode(text, maxLength);
            auto attention_mask = tokenizer.getAttentionMask(input_ids);
            
            // Run inference
            float probability = runInference(session, input_ids, attention_mask, maxLength);
            
            if (probability < 0) {
                std::cout << "❌ Inference failed" << std::endl;
                std::cout << std::endl;
                continue;
            }
            
            std::string label = probability >= threshold ? "AI-generated" : "Human-written";
            
            std::cout << "AI Probability: " << (probability * 100.0f) << "%" << std::endl;
            std::cout << "Prediction: " << label << std::endl;
            std::cout << "Expected: " << expectedLabel << std::endl;
            
            if (label == expectedLabel) {
                std::cout << "✅ PASS" << std::endl;
            } else {
                std::cout << "⚠️  FAIL (but model might be correct - labels are approximate)" << std::endl;
            }
            std::cout << std::endl;
        }
        
        std::cout << "[4/4] Interactive test" << std::endl;
        std::cout << "Enter text to analyze (or 'quit' to exit):" << std::endl;
        std::cout << "> ";
        
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "quit" || line == "exit" || line == "q") {
                break;
            }
            
            if (line.empty() || line.length() < 10) {
                std::cout << "Text too short (min 10 chars)" << std::endl;
                std::cout << "> ";
                continue;
            }
            
            auto input_ids = tokenizer.encode(line, maxLength);
            auto attention_mask = tokenizer.getAttentionMask(input_ids);
            float probability = runInference(session, input_ids, attention_mask, maxLength);
            
            if (probability >= 0) {
                std::string label = probability >= threshold ? "AI-generated" : "Human-written";
                std::cout << "AI Probability: " << (probability * 100.0f) << "%" << std::endl;
                std::cout << "Prediction: " << label << std::endl;
            }
            
            std::cout << "> ";
        }
        
        std::cout << std::endl;
        std::cout << "✅ Test complete!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
#endif
}
