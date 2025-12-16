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
    , tokenExpiresAt_(std::chrono::steady_clock::now())
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
    // If no credentials, operate in public JSON mode (previous behavior)
    if (clientId_.empty() || clientSecret_.empty()) {
        accessToken_.clear();
        return;
    }

    // Refresh OAuth token if missing or expired (simple client_credentials flow)
    if (!accessToken_.empty() && std::chrono::steady_clock::now() < tokenExpiresAt_) {
        return;
    }

    HttpRequest req;
    req.url = "https://www.reddit.com/api/v1/access_token";
    req.method = "POST";
    req.headers["User-Agent"] = userAgent_;
    req.headers["Content-Type"] = "application/x-www-form-urlencoded";

    // Basic auth header
    QByteArray creds = QString::fromStdString(clientId_ + ":" + clientSecret_).toUtf8().toBase64();
    req.headers["Authorization"] = "Basic " + creds.toStdString();
    req.body = "grant_type=client_credentials&duration=permanent";

    try {
        HttpResponse response = httpClient_->post(req);
        if (!response.success || response.statusCode != 200) {
            Logger::warn("Failed to obtain Reddit OAuth token: " + response.errorMessage);
            accessToken_.clear();
            return;
        }

        auto json = nlohmann::json::parse(response.body);
        accessToken_ = json.value("access_token", "");
        int expiresIn = json.value("expires_in", 3600);
        tokenExpiresAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(expiresIn - 60);
        Logger::info("Reddit OAuth token acquired");
    } catch (const std::exception& e) {
        Logger::error("Exception acquiring Reddit token: " + std::string(e.what()));
        accessToken_.clear();
    }
}

std::vector<ContentItem> RedditScraper::fetchPosts(const std::string& subreddit) {
    std::vector<ContentItem> items;
    
    rateLimiter_->waitIfNeeded();
    authenticate();
    
    const bool useOAuth = !accessToken_.empty();
    std::string url = useOAuth
        ? "https://oauth.reddit.com/r/" + subreddit + "/new.json?limit=25"
        : "https://www.reddit.com/r/" + subreddit + "/new.json?limit=25";
    
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    req.headers["User-Agent"] = userAgent_;
    if (useOAuth) {
        req.headers["Authorization"] = "Bearer " + accessToken_;
    }
    
    try {
        Logger::info("Fetching from URL: " + url);
        HttpResponse response = httpClient_->get(url, req.headers);
        
        Logger::info("Response status: " + std::to_string(response.statusCode) + ", success: " + (response.success ? "true" : "false"));
        
        if (response.success && response.statusCode == 200) {
            // Log first 500 chars of response for debugging
            std::string preview = response.body.substr(0, std::min(size_t(500), response.body.size()));
            Logger::info("Response preview: " + preview);
            
            auto json = nlohmann::json::parse(response.body);
            
            if (json.contains("data") && json["data"].contains("children")) {
                auto& children = json["data"]["children"];
                Logger::info("Found " + std::to_string(children.size()) + " children in response");
                
                for (const auto& child : children) {
                    if (child.contains("data")) {
                        ContentItem item = parsePost(child["data"]);
                        items.push_back(item);
                    }
                }
                Logger::info("Fetched " + std::to_string(items.size()) + " posts from r/" + subreddit);
            } else {
                Logger::warn("No data/children in Reddit response for r/" + subreddit);
                Logger::warn("Response body: " + response.body.substr(0, 1000));
            }
        } else {
            Logger::warn("Failed to fetch posts from " + subreddit + ": HTTP " + std::to_string(response.statusCode) + " - " + response.errorMessage);
            if (!response.body.empty()) {
                Logger::warn("Error response body: " + response.body.substr(0, 500));
            }
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
        Logger::info("Got " + std::to_string(posts.size()) + " posts to process");
        for (const auto& item : posts) {
            if (onItemScraped_) {
                onItemScraped_(item);
            }
        }
        
        auto comments = fetchComments(subreddit);
        Logger::info("Got " + std::to_string(comments.size()) + " comments to process");
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
    authenticate();
    
    const bool useOAuth = !accessToken_.empty();
    std::string url = useOAuth
        ? "https://oauth.reddit.com/r/" + subreddit + "/comments.json?limit=25"
        : "https://www.reddit.com/r/" + subreddit + "/comments.json?limit=25";
    
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    req.headers["User-Agent"] = userAgent_;
    if (useOAuth) {
        req.headers["Authorization"] = "Bearer " + accessToken_;
    }
    
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

