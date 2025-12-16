#include "core/AppController.h"
#include "storage/JsonlStorage.h"
#include "utils/Logger.h"
#include "utils/Crypto.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QCoreApplication>

namespace ModAI {

AppController::AppController(QObject* parent) : QObject(parent) {
    initialize();
}

AppController::~AppController() {
    if (scraper_) {
        scraper_->stop();
    }
}

void AppController::initialize() {
    // Initialize components
    dataPath_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString() + "/data";
    QDir().mkpath(QString::fromStdString(dataPath_));
    
    Logger::init(dataPath_ + "/logs/system.log");
    Logger::info("AppController initialized");
    
    // Load API keys
    std::string hiveKey = Crypto::getApiKey("hive_api_key");
    std::string redditClientId = Crypto::getApiKey("reddit_client_id");
    std::string redditClientSecret = Crypto::getApiKey("reddit_client_secret");
    
    if (hiveKey.empty()) {
        Logger::warn("No Hive API key - moderation disabled");
    }
    
    // Initialize local AI model
    std::string modelPath = dataPath_ + "/models/ai_detector.onnx";
    std::string tokenizerPath = dataPath_ + "/models";
    
    std::unique_ptr<TextDetector> textDetector;
    auto localDetector = std::make_unique<LocalAIDetector>(modelPath, tokenizerPath, 768, 0.5f);
    
    if (localDetector->isAvailable()) {
        Logger::info("Local ONNX AI detector initialized");
        textDetector = std::move(localDetector);
    } else {
        Logger::error("Local AI model not available.");
        // Create a disabled detector
        textDetector = std::make_unique<LocalAIDetector>(modelPath, tokenizerPath, 768, 0.5f);
    }
    
    auto imageModerator = std::make_unique<HiveImageModerator>(
        std::make_unique<QtHttpClient>(this), hiveKey);
    auto textModerator = std::make_unique<HiveTextModerator>(
        std::make_unique<QtHttpClient>(this), hiveKey);
    
    // Create rule engine
    auto ruleEngine = std::make_unique<RuleEngine>();
    std::string rulesPath = dataPath_ + "/rules.json";
    
    if (!QFile::exists(QString::fromStdString(rulesPath))) {
        QFile defaultRules("config/rules.json");
        if (defaultRules.exists()) {
            defaultRules.copy(QString::fromStdString(rulesPath));
        }
    }
    
    ruleEngine->loadRulesFromJson(rulesPath);
    
    // Create storage
    auto storage = std::make_unique<JsonlStorage>(dataPath_);
    storagePtr_ = storage.get();
    
    // Create cache
    std::string cachePath = dataPath_ + "/cache/results.jsonl";
    QDir().mkpath(QString::fromStdString(dataPath_ + "/cache"));
    auto cache = std::make_unique<ResultCache>(cachePath);
    
    // Create moderation engine
    moderationEngine_ = std::make_unique<ModerationEngine>(
        std::move(textDetector),
        std::move(imageModerator),
        std::move(textModerator),
        std::move(ruleEngine),
        std::move(storage),
        std::move(cache)
    );
    
    moderationEngine_->setOnItemProcessed([this](const ContentItem& item) {
        // Ensure we are on the main thread
        QTimer::singleShot(0, this, [this, item]() {
            onItemProcessed(item);
        });
    });
    
    // Create scraper
    scraper_ = std::make_unique<RedditScraper>(
        std::make_unique<QtHttpClient>(this),
        redditClientId,
        redditClientSecret,
        "ModAI/1.0 by /u/yourusername",
        dataPath_
    );
    
    scraper_->setOnItemScraped([this](const ContentItem& item) {
        QMetaObject::invokeMethod(this, "onItemScraped", Qt::QueuedConnection,
                                  Q_ARG(ModAI::ContentItem, item));
    });
}

void AppController::onItemScraped(const ContentItem& item) {
    moderationEngine_->processItem(item);
}

void AppController::onItemProcessed(const ContentItem& item) {
    itemsProcessedCount_++;
    Logger::info("Processed item: " + item.id);
}

AppController::Status AppController::getStatus() const {
    return {
        scrapingActive_,
        currentSubreddit_,
        0, // Queue size not easily accessible from ModerationEngine currently, would need to add getter
        itemsProcessedCount_
    };
}

void AppController::startScraping(const std::string& subreddit) {
    if (scrapingActive_) return;
    
    std::vector<std::string> subreddits = {subreddit};
    scraper_->setSubreddits(subreddits);
    scraper_->start(60);
    
    scrapingActive_ = true;
    currentSubreddit_ = subreddit;
    Logger::info("Started scraping: " + subreddit);
}

void AppController::stopScraping() {
    if (!scrapingActive_) return;
    
    scraper_->stop();
    scrapingActive_ = false;
    Logger::info("Stopped scraping");
}

std::vector<ContentItem> AppController::getItems(int page, int limit, const std::string& statusFilter, const std::string& search) {
    if (!storagePtr_) return {};
    
    auto allItems = storagePtr_->loadAllContent();
    std::vector<ContentItem> filtered;
    
    // Apply filters
    for (const auto& item : allItems) {
        bool matchesStatus = true;
        if (statusFilter != "all" && !statusFilter.empty()) {
            if (statusFilter == "blocked" && item.decision.auto_action != "block") matchesStatus = false;
            else if (statusFilter == "allowed" && item.decision.auto_action != "allow") matchesStatus = false;
            else if (statusFilter == "review" && item.decision.auto_action != "review") matchesStatus = false;
        }
        
        bool matchesSearch = true;
        if (!search.empty()) {
            QString qSearch = QString::fromStdString(search).toLower();
            QString qText = QString::fromStdString(item.text.value_or("")).toLower();
            QString qTitle = QString::fromStdString(item.subreddit).toLower(); // Using subreddit as proxy for title if title missing
            if (!qText.contains(qSearch) && !qTitle.contains(qSearch)) {
                matchesSearch = false;
            }
        }
        
        if (matchesStatus && matchesSearch) {
            filtered.push_back(item);
        }
    }
    
    // Sort by timestamp desc
    std::sort(filtered.begin(), filtered.end(), [](const ContentItem& a, const ContentItem& b) {
        return a.timestamp > b.timestamp;
    });
    
    // Pagination
    int start = (page - 1) * limit;
    if (start >= filtered.size()) return {};
    
    int end = std::min((int)filtered.size(), start + limit);
    return std::vector<ContentItem>(filtered.begin() + start, filtered.begin() + end);
}

std::optional<ContentItem> AppController::getItem(const std::string& id) {
    if (!storagePtr_) return std::nullopt;
    auto allItems = storagePtr_->loadAllContent();
    for (const auto& item : allItems) {
        if (item.id == id) return item;
    }
    return std::nullopt;
}

void AppController::submitAction(const std::string& id, const std::string& action, const std::string& reason) {
    if (!storagePtr_) return;
    
    // Find item to get current status
    auto itemOpt = getItem(id);
    if (!itemOpt) return;
    
    HumanAction humanAction;
    humanAction.action_id = QUuid::createUuid().toString().toStdString();
    humanAction.content_id = id;
    humanAction.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
    humanAction.reviewer = "web_user";
    humanAction.previous_status = itemOpt->decision.auto_action;
    humanAction.new_status = action;
    humanAction.reason = reason;
    
    storagePtr_->saveAction(humanAction);
    
    // Update item in storage (hacky, ideally storage should handle update)
    // For now, we just save the action. The frontend will need to merge this info or we update the item here.
    // Let's update the item in memory and save it back.
    ContentItem updatedItem = *itemOpt;
    updatedItem.decision.auto_action = action;
    storagePtr_->saveContent(updatedItem); // This might duplicate if storage is append-only.
    // JsonlStorage usually appends. So loading will need to dedupe or take latest.
}

AppController::Stats AppController::getStats() {
    if (!storagePtr_) return {0,0,0,0};
    
    auto items = storagePtr_->loadAllContent();
    Stats stats = {0,0,0,0};
    stats.total_scanned = items.size();
    
    for (const auto& item : items) {
        if (item.decision.auto_action == "block") stats.flagged_count++;
        if (item.decision.auto_action == "review") stats.action_needed++;
        if (item.ai_detection.label == "ai_generated") stats.ai_generated_count++;
    }
    return stats;
}

} // namespace ModAI
