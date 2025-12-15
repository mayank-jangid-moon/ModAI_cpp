#include "scraper/RedditScraper.h"
#include "network/HttpClient.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <QUrlQuery>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <chrono>
#include <algorithm>

namespace ModAI {

RedditScraper::RedditScraper(std::unique_ptr<HttpClient> httpClient,
                             const std::string& clientId,
                             const std::string& clientSecret,
                             const std::string& userAgent,
                             const std::string& storagePath,
                             QObject* parent)
    : QObject(parent)
    , httpClient_(std::move(httpClient))
    , clientId_(clientId)
    , clientSecret_(clientSecret)
    , userAgent_(userAgent)
    , storagePath_(storagePath)
    , rateLimiter_(std::make_unique<RateLimiter>(60, std::chrono::seconds(60)))
    , isRunning_(false) {
    
    // Ensure images directory exists
    QDir dir(QString::fromStdString(storagePath_));
    if (!dir.exists("images")) {
        dir.mkdir("images");
    }

    scrapeTimer_ = new QTimer(this);
    connect(scrapeTimer_, &QTimer::timeout, this, &RedditScraper::performScrape);
}

void RedditScraper::authenticate() {
    // No authentication needed for public JSON API
    // Reddit allows accessing public subreddit data via .json endpoint
    Logger::info("Using public Reddit JSON API (no authentication required)");
}

std::vector<ContentItem> RedditScraper::fetchPosts(const std::string& subreddit) {
    std::vector<ContentItem> items;
    
    rateLimiter_->waitIfNeeded();
    
    // Use public JSON API - no authentication needed
    std::string url = "https://www.reddit.com/r/" + subreddit + "/new.json?limit=25";
    
    HttpRequest req;
    req.url = url;
    req.method = "GET";
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
            
            // Download image
            std::string localPath = downloadImage(url);
            if (!localPath.empty()) {
                item.image_path = localPath;
            } else {
                item.image_path = url;
            }
        } else {
            item.content_type = "text";
            if (postJson.contains("title")) {
                item.text = postJson["title"].get<std::string>();
            }
        }
    }
    
    return item;
}

std::string RedditScraper::downloadImage(const std::string& url) {
    // Generate filename from hash of URL
    QByteArray hash = QCryptographicHash::hash(QByteArray::fromStdString(url), QCryptographicHash::Md5);
    std::string filename = hash.toHex().toStdString();
    
    // Determine extension
    std::string ext = ".jpg";
    if (url.find(".png") != std::string::npos) ext = ".png";
    else if (url.find(".jpeg") != std::string::npos) ext = ".jpg";
    
    std::string relativePath = "images/" + filename + ext;
    std::string fullPath = storagePath_ + "/" + relativePath;
    
    // Check if already exists
    if (QFile::exists(QString::fromStdString(fullPath))) {
        return fullPath;
    }
    
    // Download
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    
    try {
        HttpResponse response = httpClient_->get(url, req.headers);
        if (response.success && response.statusCode == 200) {
            QFile file(QString::fromStdString(fullPath));
            if (file.open(QIODevice::WriteOnly)) {
                file.write(response.body.data(), response.body.size());
                file.close();
                return fullPath;
            } else {
                Logger::error("Failed to save image to " + fullPath);
            }
        } else {
            Logger::warn("Failed to download image from " + url);
        }
    } catch (const std::exception& e) {
        Logger::error("Exception downloading image: " + std::string(e.what()));
    }
    
    return ""; // Return empty if failed
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
        auto posts = fetchPosts(subreddit);
        for (const auto& item : posts) {
            if (onItemScraped_) {
                onItemScraped_(item);
            }
        }
        
        auto comments = fetchComments(subreddit);
        for (const auto& item : comments) {
            if (onItemScraped_) {
                onItemScraped_(item);
            }
        }
    }
}

std::vector<ContentItem> RedditScraper::fetchComments(const std::string& subreddit) {
    std::vector<ContentItem> items;
    
    rateLimiter_->waitIfNeeded();
    
    std::string url = "https://www.reddit.com/r/" + subreddit + "/comments.json?limit=25";
    
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    req.headers["User-Agent"] = userAgent_;
    
    try {
        HttpResponse response = httpClient_->get(url, req.headers);
        
        if (response.success && response.statusCode == 200) {
            auto json = nlohmann::json::parse(response.body);
            
            if (json.contains("data") && json["data"].contains("children")) {
                for (const auto& child : json["data"]["children"]) {
                    if (child.contains("data")) {
                        ContentItem item = parseComment(child["data"]);
                        items.push_back(item);
                    }
                }
            }
        } else {
            Logger::warn("Failed to fetch comments from " + subreddit + ": " + response.errorMessage);
        }
    } catch (const std::exception& e) {
        Logger::error("Exception fetching comments: " + std::string(e.what()));
    }
    
    return items;
}

ContentItem RedditScraper::parseComment(const nlohmann::json& commentJson) {
    ContentItem item;
    item.subreddit = commentJson.value("subreddit", "");
    item.source = "reddit_comment";
    item.id = commentJson.value("name", ""); // t1_...
    
    if (commentJson.contains("author")) {
        item.author = commentJson["author"].get<std::string>();
    }
    
    item.content_type = "text";
    if (commentJson.contains("body")) {
        item.text = commentJson["body"].get<std::string>();
    }
    
    // Timestamp
    if (commentJson.contains("created_utc")) {
        double utc = commentJson["created_utc"].get<double>();
        item.timestamp = std::to_string(utc);
    }
    
    return item;
}

} // namespace ModAI

