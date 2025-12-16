#include "scraper/RedditScraper.h"
#include "network/HttpClient.h"
#include "utils/Logger.h"
#include "utils/Crypto.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

// Base64 helper
static std::string base64_encode(const std::string &in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

namespace ModAI {

RedditScraper::RedditScraper(std::unique_ptr<HttpClient> httpClient,
                             std::unique_ptr<Scheduler> scheduler,
                             const std::string& clientId,
                             const std::string& clientSecret,
                             const std::string& userAgent,
                             const std::string& storagePath)
    : httpClient_(std::move(httpClient))
    , scheduler_(std::move(scheduler))
    , clientId_(clientId)
    , clientSecret_(clientSecret)
    , userAgent_(userAgent)
    , storagePath_(storagePath)
    , tokenExpiresAt_(std::chrono::steady_clock::now())
    , rateLimiter_(std::make_unique<RateLimiter>(60, std::chrono::seconds(60)))
    , isRunning_(false) {
    
    // Ensure images directory exists
    std::filesystem::path dir(storagePath_);
    std::filesystem::path imgDir = dir / "images";
    if (!std::filesystem::exists(imgDir)) {
        std::filesystem::create_directories(imgDir);
    }
}

void RedditScraper::authenticate() {
    if (clientId_.empty() || clientSecret_.empty()) {
        accessToken_.clear();
        return;
    }

    if (!accessToken_.empty() && std::chrono::steady_clock::now() < tokenExpiresAt_) {
        return;
    }

    HttpRequest req;
    req.url = "https://www.reddit.com/api/v1/access_token";
    req.method = "POST";
    req.headers["User-Agent"] = userAgent_;
    req.headers["Content-Type"] = "application/x-www-form-urlencoded";

    std::string creds = base64_encode(clientId_ + ":" + clientSecret_);
    req.headers["Authorization"] = "Basic " + creds;
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
        HttpResponse response = httpClient_->get(url, req.headers);
        
        if (response.success && response.statusCode == 200) {
            auto json = nlohmann::json::parse(response.body);
            
            if (json.contains("data") && json["data"].contains("children")) {
                auto& children = json["data"]["children"];
                for (const auto& child : children) {
                    if (child.contains("data")) {
                        ContentItem item = parsePost(child["data"]);
                        items.push_back(item);
                    }
                }
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
    item.id = postJson.value("name", "");
    
    if (postJson.contains("author")) {
        item.author = postJson["author"].get<std::string>();
    }
    
    if (postJson.contains("is_self") && postJson["is_self"].get<bool>()) {
        item.content_type = "text";
        if (postJson.contains("selftext")) {
            item.text = postJson["selftext"].get<std::string>();
        }
    } else {
        std::string url = postJson.value("url", "");
        if (url.find(".jpg") != std::string::npos || 
            url.find(".png") != std::string::npos ||
            url.find(".jpeg") != std::string::npos ||
            url.find("i.redd.it") != std::string::npos) {
            item.content_type = "image";
            
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
    
    if (postJson.contains("created_utc")) {
        double utc = postJson["created_utc"].get<double>();
        item.timestamp = std::to_string(utc);
    }
    
    return item;
}

std::string RedditScraper::downloadImage(const std::string& url) {
    std::vector<uint8_t> urlBytes(url.begin(), url.end());
    std::string filename = Crypto::sha256(urlBytes);
    
    std::string ext = ".jpg";
    if (url.find(".png") != std::string::npos) ext = ".png";
    else if (url.find(".jpeg") != std::string::npos) ext = ".jpg";
    
    std::string relativePath = "images/" + filename + ext;
    std::string fullPath = storagePath_ + "/" + relativePath;
    
    if (std::filesystem::exists(fullPath)) {
        return fullPath;
    }
    
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    
    try {
        HttpResponse response = httpClient_->get(url, req.headers);
        if (response.success && response.statusCode == 200) {
            std::ofstream file(fullPath, std::ios::binary);
            if (file.is_open()) {
                file.write(response.body.data(), response.body.size());
                file.close();
                return fullPath;
            } else {
                Logger::error("Failed to save image to " + fullPath);
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Exception downloading image: " + std::string(e.what()));
    }
    
    return "";
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
    item.id = commentJson.value("name", "");
    
    if (commentJson.contains("author")) {
        item.author = commentJson["author"].get<std::string>();
    }
    
    item.content_type = "text";
    if (commentJson.contains("body")) {
        item.text = commentJson["body"].get<std::string>();
    }
    
    if (commentJson.contains("created_utc")) {
        double utc = commentJson["created_utc"].get<double>();
        item.timestamp = std::to_string(utc);
    }
    
    return item;
}

void RedditScraper::setSubreddits(const std::vector<std::string>& subreddits) {
    subreddits_ = subreddits;
}

void RedditScraper::start(int intervalSeconds) {
    if (isRunning_) return;
    isRunning_ = true;
    
    scheduler_->schedule(std::chrono::seconds(intervalSeconds), [this]() {
        this->performScrape();
    });
    
    Logger::info("Scraper started");
}

void RedditScraper::stop() {
    if (!isRunning_) return;
    isRunning_ = false;
    scheduler_->stop();
    Logger::info("Scraper stopped");
}

void RedditScraper::setOnItemScraped(std::function<void(const ContentItem&)> callback) {
    onItemScraped_ = callback;
}

void RedditScraper::performScrape() {
    if (!isRunning_) return;
    
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

} // namespace ModAI

