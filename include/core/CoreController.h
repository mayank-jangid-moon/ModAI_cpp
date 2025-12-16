#pragma once

#include "core/ModerationEngine.h"
#include "scraper/RedditScraper.h"
#include "storage/Storage.h"
#include "core/interfaces/HttpServerInterface.h"
#include <memory>
#include <string>

namespace ModAI {

class CoreController {
private:
    std::unique_ptr<ModerationEngine> moderationEngine_;
    std::unique_ptr<RedditScraper> scraper_;
    Storage* storagePtr_; // Owned by moderationEngine_
    std::unique_ptr<HttpServerInterface> server_;
    
    std::string dataPath_;
    bool scrapingActive_{false};
    std::string currentSubreddit_;
    int itemsProcessedCount_{0};

    void initialize();
    void setupRoutes();

public:
    CoreController();
    ~CoreController();
    
    void run();
};

} // namespace ModAI
