#pragma once

#include <QObject>
#include <memory>
#include <string>
#include "core/ModerationEngine.h"
#include "scraper/RedditScraper.h"
#include "storage/Storage.h"
#include "core/ResultCache.h"
#include "core/RuleEngine.h"
#include "detectors/LocalAIDetector.h"
#include "detectors/HiveImageModerator.h"
#include "detectors/HiveTextModerator.h"
#include "network/QtHttpClient.h"

namespace ModAI {

class AppController : public QObject {
    Q_OBJECT

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController();

    // API Methods
    struct Status {
        bool scraping_active;
        std::string current_subreddit;
        int queue_size;
        int items_processed;
    };

    Status getStatus() const;
    void startScraping(const std::string& subreddit);
    void stopScraping();
    
    std::vector<ContentItem> getItems(int page, int limit, const std::string& statusFilter, const std::string& search);
    std::optional<ContentItem> getItem(const std::string& id);
    void submitAction(const std::string& id, const std::string& action, const std::string& reason);
    
    // Stats
    struct Stats {
        int total_scanned;
        int flagged_count;
        int action_needed;
        int ai_generated_count;
    };
    Stats getStats();

private slots:
    void onItemProcessed(const ContentItem& item);
    void onItemScraped(const ContentItem& item);

private:
    std::unique_ptr<ModerationEngine> moderationEngine_;
    std::unique_ptr<RedditScraper> scraper_;
    Storage* storagePtr_{nullptr}; // Owned by moderationEngine_ unique_ptr move? No, usually shared or raw ptr if owned elsewhere. 
    // In MainWindow, storage was unique_ptr moved into ModerationEngine, but raw ptr kept.
    
    std::string dataPath_;
    bool scrapingActive_{false};
    std::string currentSubreddit_;
    int itemsProcessedCount_{0};

    void initialize();
};

} // namespace ModAI
