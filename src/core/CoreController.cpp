#include "core/CoreController.h"
#include "network/CppHttpServer.h"
#include "network/CurlHttpClient.h"
#include "core/services/StdScheduler.h"
#include "storage/JsonlStorage.h"
#include "utils/Logger.h"
#include "utils/Crypto.h"
#include "detectors/LocalAIDetector.h"
#include "detectors/HiveImageModerator.h"
#include "detectors/HiveTextModerator.h"
#include <filesystem>
#include <nlohmann/json.hpp>

namespace ModAI {

CoreController::CoreController() {
    initialize();
}

CoreController::~CoreController() {
    if (scraper_) scraper_->stop();
    if (server_) server_->stop();
}

void CoreController::initialize() {
    // Path setup
    const char* home = std::getenv("HOME");
    std::string homeDir = home ? home : ".";
    dataPath_ = homeDir + "/.local/share/ModAI/data";
    
    std::filesystem::create_directories(dataPath_);
    Logger::init(dataPath_ + "/logs/system.log");
    Logger::info("CoreController initialized (Qt-free)");

    // Load Keys
    std::string hiveKey = Crypto::getApiKey("hive_api_key");
    std::string redditClientId = Crypto::getApiKey("reddit_client_id");
    std::string redditClientSecret = Crypto::getApiKey("reddit_client_secret");

    // Components
    auto storage = std::make_unique<JsonlStorage>(dataPath_);
    storagePtr_ = storage.get();
    
    auto cache = std::make_unique<ResultCache>(dataPath_ + "/cache/results.jsonl");
    auto ruleEngine = std::make_unique<RuleEngine>();
    ruleEngine->loadRulesFromJson(dataPath_ + "/rules.json");

    // Detectors (Mocking/Simplifying for now as they might have Qt deps inside)
    // We need to ensure Detectors are Qt-free. 
    // Assuming LocalAIDetector is safe (ONNX).
    // Assuming Hive moderators are safe if they use HttpClient interface.
    
    // For now, let's construct them.
    std::string modelPath = dataPath_ + "/models/ai_detector.onnx";
    std::string tokenizerPath = dataPath_ + "/models";
    auto textDetector = std::make_unique<LocalAIDetector>(modelPath, tokenizerPath, 768, 0.5f);
    
    auto imageModerator = std::make_unique<HiveImageModerator>(
        std::make_unique<CurlHttpClient>(), hiveKey);
    auto textModerator = std::make_unique<HiveTextModerator>(
        std::make_unique<CurlHttpClient>(), hiveKey);

    moderationEngine_ = std::make_unique<ModerationEngine>(
        std::move(textDetector),
        std::move(imageModerator),
        std::move(textModerator),
        std::move(ruleEngine),
        std::move(storage),
        std::move(cache)
    );

    moderationEngine_->setOnItemProcessed([this](const ContentItem& item) {
        itemsProcessedCount_++;
        Logger::info("Processed: " + item.id);
    });

    // Scraper
    scraper_ = std::make_unique<RedditScraper>(
        std::make_unique<CurlHttpClient>(),
        std::make_unique<StdScheduler>(),
        redditClientId,
        redditClientSecret,
        "ModAI/1.0 Core",
        dataPath_
    );

    scraper_->setOnItemScraped([this](const ContentItem& item) {
        moderationEngine_->processItem(item);
    });

    // Server
    server_ = std::make_unique<CppHttpServer>(8080);
    setupRoutes();
}

void CoreController::setupRoutes() {
    server_->registerHandler("GET", "/api/status", [this](const ServerRequest&) {
        nlohmann::json j;
        j["scraping_active"] = scraper_->isScraping();
        j["items_processed"] = itemsProcessedCount_;
        ServerResponse res;
        res.body = j.dump();
        return res;
    });

    server_->registerHandler("POST", "/api/scraper/start", [this](const ServerRequest& req) {
        auto j = nlohmann::json::parse(req.body);
        std::string sub = j.value("subreddit", "");
        if (!sub.empty()) {
            scraper_->setSubreddits({sub});
            scraper_->start();
            currentSubreddit_ = sub;
        }
        ServerResponse res;
        res.body = "{\"status\":\"started\"}";
        return res;
    });

    server_->registerHandler("POST", "/api/scraper/stop", [this](const ServerRequest&) {
        scraper_->stop();
        ServerResponse res;
        res.body = "{\"status\":\"stopped\"}";
        return res;
    });
    
    server_->registerHandler("GET", "/api/items", [this](const ServerRequest& req) {
        auto items = storagePtr_->loadAllContent();
        // Simple pagination logic here...
        nlohmann::json j = nlohmann::json::array();
        for(const auto& item : items) {
            j.push_back(nlohmann::json::parse(item.toJson()));
        }
        nlohmann::json resp;
        resp["data"] = j;
        ServerResponse res;
        res.body = resp.dump();
        return res;
    });
}

void CoreController::run() {
    server_->start();
    // Block main thread
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace ModAI
