#pragma once

#include "core/ModerationEngine.h"
#include "storage/Storage.h"
#include "scraper/RedditScraper.h"
#include <memory>
#include <string>
#include <vector>

namespace ModAI {

class ApiServer {
private:
    std::shared_ptr<ModerationEngine> moderationEngine_;
    std::shared_ptr<Storage> storage_;
    std::shared_ptr<RedditScraper> redditScraper_;
    int port_;
    std::string dataPath_;
    
    // Chat history for the chatbot
    struct ChatMessage {
        std::string id;
        std::string role; // "user" or "assistant"
        std::string content;
        long timestamp;
        bool was_blocked;
        std::string block_reason;
    };
    std::vector<ChatMessage> chatHistory_;
    
public:
    ApiServer(std::shared_ptr<ModerationEngine> moderationEngine,
              std::shared_ptr<Storage> storage,
              std::shared_ptr<RedditScraper> redditScraper = nullptr,
              int port = 8080,
              const std::string& dataPath = "./data");
    
    void start();
    void stop();
};

} // namespace ModAI
