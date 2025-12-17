#pragma once

#include "core/ContentItem.h"
#include "network/HttpClient.h"
#include "network/RateLimiter.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

namespace ModAI {

class RedditScraper {
private:
    std::unique_ptr<HttpClient> httpClient_;
    std::string clientId_;
    std::string clientSecret_;
    std::string userAgent_;
    std::string accessToken_;
    std::chrono::steady_clock::time_point tokenExpiresAt_;
    std::string storagePath_;
    std::unique_ptr<RateLimiter> rateLimiter_;
    
    std::vector<std::string> subreddits_;
    std::atomic<bool> isRunning_;
    std::thread scrapeThread_;
    std::mutex mutex_;
    int intervalSeconds_;
    
    std::function<void(const ContentItem&)> onItemScraped_;
    std::vector<ContentItem> lastScrapedItems_;

    void authenticate();
    std::vector<ContentItem> fetchPosts(const std::string& subreddit, const std::string& timeFilter = "day", int limit = 25);
    std::vector<ContentItem> fetchComments(const std::string& subreddit);
    ContentItem parsePost(const nlohmann::json& postJson);
    ContentItem parseComment(const nlohmann::json& commentJson);
    void parseCommentsRecursive(const nlohmann::json& children, std::vector<ContentItem>& items);
    std::string downloadImage(const std::string& url);
    void scrapeLoop();

public:
    RedditScraper(std::unique_ptr<HttpClient> httpClient,
                  const std::string& clientId,
                  const std::string& clientSecret,
                  const std::string& userAgent,
                  const std::string& storagePath);
    
    ~RedditScraper();
    
    void setSubreddits(const std::vector<std::string>& subreddits);
    std::vector<std::string> getSubreddits() const;
    
    void start(int intervalSeconds = 60);
    void stop();
    bool isScraping() const { return isRunning_; }
    
    void setOnItemScraped(std::function<void(const ContentItem&)> callback);
    
    // Fetch comments for a specific post
    std::vector<ContentItem> fetchPostComments(const std::string& subreddit, const std::string& postId);
    
    // Get last scraped items (for API endpoints)
    std::vector<ContentItem> getLastScrapedItems() const;
    
    // Manual scrape (non-blocking)
    std::vector<ContentItem> scrapeOnce();
};

} // namespace ModAI
