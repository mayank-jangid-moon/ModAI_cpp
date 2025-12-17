#include "api/ApiServer.h"
#include "core/ModerationEngine.h"
#include "core/RuleEngine.h"
#include "core/ResultCache.h"
#include "detectors/LocalAIDetector.h"
#include "detectors/HiveImageModerator.h"
#include "detectors/HiveTextModerator.h"
#include "network/CurlHttpClient.h"
#include "storage/JsonlStorage.h"
#include "scraper/RedditScraper.h"
#include "utils/Logger.h"
#include "utils/Crypto.h"
#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    using namespace ModAI;
    
    // Parse command line arguments
    int port = 8080;
    std::string dataPath = "./data";
    std::string rulesPath = "./config/rules.json";
    std::string modelPath = "";
    std::string tokenizerPath = "";
    bool enableReddit = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--data" && i + 1 < argc) {
            dataPath = argv[++i];
        } else if (arg == "--rules" && i + 1 < argc) {
            rulesPath = argv[++i];
        } else if (arg == "--model" && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (arg == "--tokenizer" && i + 1 < argc) {
            tokenizerPath = argv[++i];
        } else if (arg == "--enable-reddit") {
            enableReddit = true;
        } else if (arg == "--help") {
            std::cout << "ModAI Backend Server\n\n";
            std::cout << "Usage: " << argv[0] << " [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --port <port>           Server port (default: 8080)\n";
            std::cout << "  --data <path>           Data directory path (default: ./data)\n";
            std::cout << "  --rules <path>          Rules JSON file path (default: ./config/rules.json)\n";
            std::cout << "  --model <path>          ONNX model path for local AI detection\n";
            std::cout << "  --tokenizer <path>      Tokenizer path for local AI detection\n";
            std::cout << "  --enable-reddit         Enable Reddit scraper integration\n";
            std::cout << "  --help                  Show this help message\n";
            return 0;
        }
    }
    
    // Initialize logger
    fs::create_directories(dataPath + "/logs");
    Logger::init(dataPath + "/logs/backend.log");
    Logger::info("=== ModAI Backend Server Starting ===");
    Logger::info("Port: " + std::to_string(port));
    Logger::info("Data path: " + dataPath);
    Logger::info("Rules path: " + rulesPath);
    
    // Create necessary directories
    fs::create_directories(dataPath);
    fs::create_directories(dataPath + "/images");
    fs::create_directories(dataPath + "/cache");
    fs::create_directories(dataPath + "/exports");
    fs::create_directories(dataPath + "/uploads");
    
    // Get API keys
    std::string hiveApiKey = Crypto::getApiKey("HIVE_API_KEY");
    if (hiveApiKey.empty()) {
        Logger::warn("HIVE_API_KEY not set - Hive moderation will be disabled");
        Logger::warn("Set via environment variable: export MODAI_HIVE_API_KEY=your_key");
    }
    
    // Initialize components
    try {
        // HTTP Client
        auto httpClient = std::make_unique<CurlHttpClient>();
        httpClient->setTimeout(30000);
        httpClient->setRetries(3, 1000);
        
        // Text Detector (Local AI or fallback)
        std::unique_ptr<TextDetector> textDetector;
        if (!modelPath.empty() && !tokenizerPath.empty()) {
            Logger::info("Initializing Local AI Detector");
            Logger::info("Model: " + modelPath);
            Logger::info("Tokenizer: " + tokenizerPath);
            auto localDetector = std::make_unique<LocalAIDetector>(modelPath, tokenizerPath);
            if (localDetector->isAvailable()) {
                Logger::info("Local AI Detector initialized successfully");
                textDetector = std::move(localDetector);
            } else {
                Logger::error("Local AI Detector initialization failed - will use dummy detector");
                // Fallback to a dummy detector that always returns human
                class DummyTextDetector : public TextDetector {
                public:
                    TextDetectResult analyze(const std::string&) override {
                        TextDetectResult result;
                        result.label = "human";
                        result.ai_score = 0.0;
                        result.confidence = 0.0;
                        return result;
                    }
                };
                textDetector = std::make_unique<DummyTextDetector>();
            }
        } else {
            Logger::warn("Model/tokenizer paths not provided - using dummy text detector");
            class DummyTextDetector : public TextDetector {
            public:
                TextDetectResult analyze(const std::string&) override {
                    TextDetectResult result;
                    result.label = "human";
                    result.ai_score = 0.0;
                    result.confidence = 0.0;
                    return result;
                }
            };
            textDetector = std::make_unique<DummyTextDetector>();
        }
        
        // Image Moderator
        auto httpClientForImage = std::make_unique<CurlHttpClient>();
        auto imageModerator = std::make_unique<HiveImageModerator>(
            std::move(httpClientForImage), hiveApiKey);
        
        // Text Moderator
        auto httpClientForText = std::make_unique<CurlHttpClient>();
        auto textModerator = std::make_unique<HiveTextModerator>(
            std::move(httpClientForText), hiveApiKey);
        
        // Rule Engine
        auto ruleEngine = std::make_unique<RuleEngine>();
        if (fs::exists(rulesPath)) {
            ruleEngine->loadRulesFromJson(rulesPath);
        } else {
            Logger::warn("Rules file not found: " + rulesPath);
            Logger::warn("Using default allow-all rule");
        }
        
        // Storage
        auto storage = std::make_unique<JsonlStorage>(dataPath);
        auto storagePtr = storage.get();  // Keep raw pointer for API server
        
        // Result Cache
        auto cache = std::make_unique<ResultCache>(dataPath + "/cache/results.jsonl");
        
        // Moderation Engine
        auto moderationEngine = std::make_shared<ModerationEngine>(
            std::move(textDetector),
            std::move(imageModerator),
            std::move(textModerator),
            std::move(ruleEngine),
            std::move(storage),
            std::move(cache)
        );
        
        // Wrap storage pointer for API server
        auto storageShared = std::shared_ptr<Storage>(storagePtr, [](Storage*){});
        
        // Reddit Scraper (optional, no authentication needed for public JSON API)
        std::shared_ptr<RedditScraper> redditScraper = nullptr;
        if (enableReddit) {
            Logger::info("Initializing Reddit scraper");
            auto redditHttpClient = std::make_unique<CurlHttpClient>();
            redditScraper = std::make_shared<RedditScraper>(
                std::move(redditHttpClient),
                "",  // No client ID needed for public API
                "",  // No client secret needed for public API
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                dataPath
            );
            Logger::info("Reddit scraper initialized (using public JSON API)");
        }
        
        Logger::info("All components initialized successfully");
        
        // API Server
        ApiServer server(moderationEngine, storageShared, redditScraper, port, dataPath);
        
        Logger::info("Starting API server...");
        server.start();
        
    } catch (const std::exception& e) {
        Logger::error("Fatal error: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}
