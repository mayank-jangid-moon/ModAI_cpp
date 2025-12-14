#include "scraper/RedditScraper.h"
#include "network/HttpClient.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <QUrlQuery>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <chrono>
#include <algorithm>

namespace ModAI {

RedditScraper::RedditScraper(std::unique_ptr<HttpClient> httpClient,
                             const std::string& clientId,
                             const std::string& clientSecret,
                             const std::string& userAgent,
                             QObject* parent)
    : QObject(parent)
    , httpClient_(std::move(httpClient))
    , clientId_(clientId)
    , clientSecret_(clientSecret)
    , userAgent_(userAgent)
    , rateLimiter_(std::make_unique<RateLimiter>(60, std::chrono::seconds(60)))
    , isRunning_(false) {
    
    scrapeTimer_ = new QTimer(this);
    connect(scrapeTimer_, &QTimer::timeout, this, &RedditScraper::performScrape);
}

void RedditScraper::authenticate() {
    if (!accessToken_.empty()) {
        return;  // Already authenticated
    }
    
    std::string url = "https://www.reddit.com/api/v1/access_token";
    
    HttpRequest req;
    req.url = url;
    req.method = "POST";
    req.headers["User-Agent"] = userAgent_;
    req.contentType = "application/x-www-form-urlencoded";
    
    // Basic auth: clientId:clientSecret base64 encoded
    std::string auth = clientId_ + ":" + clientSecret_;
    QByteArray authBytes = QByteArray::fromStdString(auth);
    req.headers["Authorization"] = "Basic " + QByteArray::toBase64(authBytes).toStdString();
    
    req.body = "grant_type=client_credentials";
    
    try {
        HttpResponse response = httpClient_->post(req);
        
        if (response.success && response.statusCode == 200) {
            auto json = nlohmann::json::parse(response.body);
            if (json.contains("access_token")) {
                accessToken_ = json["access_token"].get<std::string>();
                Logger::info("Reddit authentication successful");
            }
        } else {
            Logger::error("Reddit authentication failed: " + response.errorMessage);
        }
    } catch (const std::exception& e) {
        Logger::error("Exception during Reddit authentication: " + std::string(e.what()));
    }
}

std::vector<ContentItem> RedditScraper::fetchPosts(const std::string& subreddit) {
    std::vector<ContentItem> items;
    
    if (accessToken_.empty()) {
        authenticate();
        if (accessToken_.empty()) {
            return items;
        }
    }
    
    rateLimiter_->waitIfNeeded();
    
    std::string url = "https://oauth.reddit.com/r/" + subreddit + "/new.json?limit=25";
    
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    req.headers["Authorization"] = "Bearer " + accessToken_;
    req.headers["User-Agent"] = userAgent_;
    
    try {
        HttpResponse response = httpClient_->get(url, req.headers);
        
        if (response.success && response.statusCode == 200) {
            auto json = nlohmann::json::parse(response.body);
            
            if (json.contains("data") && json["data"].contains("children")) {
                for (const auto& child : json["data"]["children"]) {
                    if (child.contains("data")) {
                        ContentItem item = parsePost(child["data"]);
                        items.push_back(item);
                    }
                }
            }
        } else {
            if (response.statusCode == 401) {
                // Token expired, re-authenticate
                accessToken_.clear();
                authenticate();
            }
            Logger::warn("Failed to fetch posts from " + subreddit + ": " + response.errorMessage);
        }
    } catch (const std::exception& e) {
        Logger::error("Exception fetching posts: " + std::string(e.what()));
    }
    
    return items;
}

ContentItem RedditScraper::parsePost(const nlohmann::json& postJson) {
    ContentItem item;
    item.subreddit = postJson.value("subreddit", "");
    item.source = "reddit";
    
    if (postJson.contains("author")) {
        item.author = postJson["author"].get<std::string>();
    }
    
    // Determine content type
    if (postJson.contains("is_self") && postJson["is_self"].get<bool>()) {
        item.content_type = "text";
        if (postJson.contains("selftext")) {
            item.text = postJson["selftext"].get<std::string>();
        }
    } else {
        // Check if it's an image
        std::string url = postJson.value("url", "");
        if (url.find(".jpg") != std::string::npos || 
            url.find(".png") != std::string::npos ||
            url.find(".jpeg") != std::string::npos ||
            url.find("i.redd.it") != std::string::npos) {
            item.content_type = "image";
            item.image_path = url;
        } else {
            item.content_type = "text";
            if (postJson.contains("title")) {
                item.text = postJson["title"].get<std::string>();
            }
        }
    }
    
    return item;
}

void RedditScraper::setSubreddits(const std::vector<std::string>& subreddits) {
    subreddits_ = subreddits;
}

void RedditScraper::start(int intervalSeconds) {
    if (isRunning_) {
        return;
    }
    
    isRunning_ = true;
    authenticate();
    
    // Perform initial scrape
    performScrape();
    
    // Set up timer for periodic scraping
    scrapeTimer_->start(intervalSeconds * 1000);
    
    Logger::info("Reddit scraper started");
}

void RedditScraper::stop() {
    if (!isRunning_) {
        return;
    }
    
    isRunning_ = false;
    scrapeTimer_->stop();
    
    Logger::info("Reddit scraper stopped");
}

void RedditScraper::setOnItemScraped(std::function<void(const ContentItem&)> callback) {
    onItemScraped_ = callback;
}

void RedditScraper::performScrape() {
    if (!isRunning_) {
        return;
    }
    
    Logger::info("Performing scrape of " + std::to_string(subreddits_.size()) + " subreddits");
    
    for (const auto& subreddit : subreddits_) {
        auto items = fetchPosts(subreddit);
        
        for (const auto& item : items) {
            if (onItemScraped_) {
                onItemScraped_(item);
            }
        }
    }
}

} // namespace ModAI

