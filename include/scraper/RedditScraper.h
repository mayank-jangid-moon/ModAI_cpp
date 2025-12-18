#pragma once

#include "core/ContentItem.h"
#include "network/HttpClient.h"
#include "network/RateLimiter.h"
#include "detectors/ImageModerator.h"
#include <QObject>
#include <QTimer>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include <chrono>

namespace ModAI {

class RedditScraper : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<HttpClient> httpClient_;
    std::unique_ptr<ImageModerator> imageModerator_;
    std::string clientId_;
    std::string clientSecret_;
    std::string userAgent_;
    std::string accessToken_;
    std::chrono::steady_clock::time_point tokenExpiresAt_;
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
    void parseCommentsRecursive(const nlohmann::json& children, std::vector<ContentItem>& items);
    std::string downloadImage(const std::string& url);
    void moderateImage(ContentItem& item);

public:
    RedditScraper(std::unique_ptr<HttpClient> httpClient,
                  const std::string& clientId,
                  const std::string& clientSecret,
                  const std::string& userAgent,
                  const std::string& storagePath,
                  QObject* parent = nullptr);
    
    void setImageModerator(std::unique_ptr<ImageModerator> imageModerator);
    void setSubreddits(const std::vector<std::string>& subreddits);
    void start(int intervalSeconds = 60);
    void stop();
    bool isScraping() const { return isRunning_; }
    
    void setOnItemScraped(std::function<void(const ContentItem&)> callback);
    
    // Fetch comments for a specific post
    std::vector<ContentItem> fetchPostComments(const std::string& subreddit, const std::string& postId);

private slots:
    void performScrape();
};

} // namespace ModAI

