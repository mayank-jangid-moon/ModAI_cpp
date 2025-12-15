#pragma once

#include "core/ContentItem.h"
#include "network/HttpClient.h"
#include "network/RateLimiter.h"
#include <QObject>
#include <QTimer>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace ModAI {

class RedditScraper : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<HttpClient> httpClient_;
    std::string clientId_;
    std::string clientSecret_;
    std::string userAgent_;
    std::string accessToken_;
    std::string storagePath_;
    std::unique_ptr<RateLimiter> rateLimiter_;
    QTimer* scrapeTimer_;
    
    std::vector<std::string> subreddits_;
    bool isRunning_;
    
    std::function<void(const ContentItem&)> onItemScraped_;

    void authenticate();
    std::vector<ContentItem> fetchPosts(const std::string& subreddit);
    std::vector<ContentItem> fetchComments(const std::string& subreddit);
    ContentItem parsePost(const nlohmann::json& postJson);
    ContentItem parseComment(const nlohmann::json& commentJson);
    std::string downloadImage(const std::string& url);

public:
    RedditScraper(std::unique_ptr<HttpClient> httpClient,
                  const std::string& clientId,
                  const std::string& clientSecret,
                  const std::string& userAgent,
                  const std::string& storagePath,
                  QObject* parent = nullptr);
    
    void setSubreddits(const std::vector<std::string>& subreddits);
    void start(int intervalSeconds = 60);
    void stop();
    bool isScraping() const { return isRunning_; }
    
    void setOnItemScraped(std::function<void(const ContentItem&)> callback);

private slots:
    void performScrape();
};

} // namespace ModAI

