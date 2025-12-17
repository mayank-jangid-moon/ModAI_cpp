#include "scraper/RedditScraper.h"
#include "utils/Logger.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <filesystem>

namespace ModAI {

RedditScraper::RedditScraper(std::unique_ptr<HttpClient> httpClient,
                             const std::string& clientId,
                             const std::string& clientSecret,
                             const std::string& userAgent,
                             const std::string& storagePath)
    : httpClient_(std::move(httpClient))
    , clientId_(clientId)
    , clientSecret_(clientSecret)
    , userAgent_(userAgent)
    , storagePath_(storagePath)
    , rateLimiter_(std::make_unique<RateLimiter>(60, std::chrono::milliseconds(60000))) // 60 requests per minute
    , isRunning_(false)
    , intervalSeconds_(60) {
    
    std::filesystem::create_directories(storagePath_);
}

RedditScraper::~RedditScraper() {
    stop();
}

void RedditScraper::setSubreddits(const std::vector<std::string>& subreddits) {
    std::lock_guard<std::mutex> lock(mutex_);
    subreddits_ = subreddits;
}

std::vector<std::string> RedditScraper::getSubreddits() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    return subreddits_;
}

void RedditScraper::authenticate() {
    if (clientId_.empty() || clientSecret_.empty()) {
        Logger::warn("Reddit API credentials not configured");
        return;
    }
    
    // Check if token is still valid
    auto now = std::chrono::steady_clock::now();
    if (!accessToken_.empty() && now < tokenExpiresAt_) {
        return; // Token still valid
    }
    
    std::string authUrl = "https://www.reddit.com/api/v1/access_token";
    std::string postData = "grant_type=client_credentials";
    std::string auth = clientId_ + ":" + clientSecret_;
    
    try {
        HttpRequest request;
        request.url = authUrl;
        request.method = "POST";
        request.body = postData;
        request.headers["User-Agent"] = userAgent_;
        request.headers["Content-Type"] = "application/x-www-form-urlencoded";
        request.headers["Authorization"] = "Basic " + auth; // Should be base64 encoded
        
        HttpResponse response = httpClient_->post(request);
        
        if (response.statusCode == 200) {
            nlohmann::json responseJson = nlohmann::json::parse(response.body);
            accessToken_ = responseJson["access_token"].get<std::string>();
            int expiresIn = responseJson["expires_in"].get<int>();
            
            tokenExpiresAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(expiresIn - 60);
            Logger::info("Reddit authentication successful");
        } else {
            Logger::error("Reddit authentication failed: " + std::to_string(response.statusCode));
        }
    } catch (const std::exception& e) {
        Logger::error("Reddit authentication error: " + std::string(e.what()));
    }
}

std::vector<ContentItem> RedditScraper::fetchPosts(const std::string& subreddit, 
                                                   const std::string& timeFilter, 
                                                   int limit) {
    std::vector<ContentItem> items;
    
    if (!rateLimiter_->acquire()) {
        Logger::warn("Rate limit exceeded for Reddit API");
        return items;
    }
    
    authenticate();
    
    if (accessToken_.empty()) {
        Logger::warn("No Reddit access token available");
        return items;
    }
    
    std::string url = "https://oauth.reddit.com/r/" + subreddit + "/top?t=" + timeFilter + "&limit=" + std::to_string(limit);
    std::map<std::string, std::string> headers = {
        {"User-Agent", userAgent_},
        {"Authorization", "Bearer " + accessToken_}
    };
    
    try {
        HttpResponse response = httpClient_->get(url, headers);
        
        if (response.statusCode == 200) {
            nlohmann::json responseJson = nlohmann::json::parse(response.body);
            
            if (responseJson.contains("data") && responseJson["data"].contains("children")) {
                for (const auto& child : responseJson["data"]["children"]) {
                    if (child.contains("data")) {
                        items.push_back(parsePost(child["data"]));
                    }
                }
            }
        } else {
            Logger::error("Failed to fetch Reddit posts: " + std::to_string(response.statusCode));
        }
    } catch (const std::exception& e) {
        Logger::error("Error fetching Reddit posts: " + std::string(e.what()));
    }
    
    return items;
}

ContentItem RedditScraper::parsePost(const nlohmann::json& postJson) {
    ContentItem item;
    item.source = "reddit";
    item.timestamp = std::to_string(std::time(nullptr));
    
    if (postJson.contains("id")) {
        item.content_id = postJson["id"].get<std::string>();
        item.id = postJson["id"].get<std::string>();
    }
    
    if (postJson.contains("subreddit")) {
        item.metadata["subreddit"] = postJson["subreddit"].get<std::string>();
        item.subreddit = postJson["subreddit"].get<std::string>();
    }
    
    if (postJson.contains("author")) {
        item.metadata["author"] = postJson["author"].get<std::string>();
        item.author = postJson["author"].get<std::string>();
    }
    
    if (postJson.contains("title")) {
        item.metadata["title"] = postJson["title"].get<std::string>();
    }
    
    if (postJson.contains("score")) {
        item.metadata["score"] = std::to_string(postJson["score"].get<int>());
    }
    
    if (postJson.contains("permalink")) {
        item.metadata["permalink"] = "https://reddit.com" + postJson["permalink"].get<std::string>();
    }
    
    // Text content
    if (postJson.contains("selftext") && !postJson["selftext"].is_null()) {
        std::string selftext = postJson["selftext"].get<std::string>();
        if (!selftext.empty()) {
            item.text_content = selftext;
            item.text = selftext;
            item.content_type = "text";
        }
    }
    
    // Image content
    if (postJson.contains("post_hint") && postJson["post_hint"].get<std::string>() == "image") {
        if (postJson.contains("url")) {
            std::string imageUrl = postJson["url"].get<std::string>();
            std::string localPath = downloadImage(imageUrl);
            if (!localPath.empty()) {
                item.image_path = localPath;
                item.content_type = "image";
            }
        }
    }
    
    // Both text and image
    if (item.text_content.has_value() && !item.text_content->empty() && item.image_path.has_value()) {
        item.content_type = "both";
    }
    
    return item;
}

std::string RedditScraper::downloadImage(const std::string& url) {
    try {
        std::map<std::string, std::string> headers;
        HttpResponse response = httpClient_->get(url, headers);
        
        if (response.statusCode == 200) {
            // Generate filename from URL
            std::string filename = std::to_string(std::hash<std::string>{}(url)) + ".jpg";
            std::string filepath = storagePath_ + "/images/" + filename;
            
            std::filesystem::create_directories(storagePath_ + "/images");
            
            std::ofstream file(filepath, std::ios::binary);
            file.write(response.body.c_str(), response.body.size());
            file.close();
            
            return filepath;
        }
    } catch (const std::exception& e) {
        Logger::error("Error downloading image: " + std::string(e.what()));
    }
    
    return "";
}

ContentItem RedditScraper::parseComment(const nlohmann::json& commentJson) {
    ContentItem item;
    item.source = "reddit";
    item.content_type = "text";
    item.timestamp = std::to_string(std::time(nullptr));
    
    if (commentJson.contains("id")) {
        item.content_id = commentJson["id"].get<std::string>();
        item.id = commentJson["id"].get<std::string>();
    }
    
    if (commentJson.contains("subreddit")) {
        item.metadata["subreddit"] = commentJson["subreddit"].get<std::string>();
        item.subreddit = commentJson["subreddit"].get<std::string>();
    }
    
    if (commentJson.contains("author")) {
        item.metadata["author"] = commentJson["author"].get<std::string>();
        item.author = commentJson["author"].get<std::string>();
    }
    
    if (commentJson.contains("body")) {
        item.text_content = commentJson["body"].get<std::string>();
        item.text = commentJson["body"].get<std::string>();
    }
    
    if (commentJson.contains("score")) {
        item.metadata["score"] = std::to_string(commentJson["score"].get<int>());
    }
    
    return item;
}

std::vector<ContentItem> RedditScraper::scrapeOnce() {
    std::vector<ContentItem> allItems;
    
    std::vector<std::string> subs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subs = subreddits_;
    }
    
    for (const auto& subreddit : subs) {
        Logger::info("Scraping r/" + subreddit);
        
        auto posts = fetchPosts(subreddit);
        allItems.insert(allItems.end(), posts.begin(), posts.end());
        
        if (onItemScraped_) {
            for (const auto& item : posts) {
                onItemScraped_(item);
            }
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lastScrapedItems_ = allItems;
    }
    
    return allItems;
}

void RedditScraper::scrapeLoop() {
    while (isRunning_) {
        scrapeOnce();
        
        // Sleep for the interval
        for (int i = 0; i < intervalSeconds_ && isRunning_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void RedditScraper::start(int intervalSeconds) {
    if (isRunning_) {
        Logger::warn("Reddit scraper already running");
        return;
    }
    
    intervalSeconds_ = intervalSeconds;
    isRunning_ = true;
    
    scrapeThread_ = std::thread(&RedditScraper::scrapeLoop, this);
    Logger::info("Reddit scraper started");
}

void RedditScraper::stop() {
    if (!isRunning_) {
        return;
    }
    
    isRunning_ = false;
    
    if (scrapeThread_.joinable()) {
        scrapeThread_.join();
    }
    
    Logger::info("Reddit scraper stopped");
}

void RedditScraper::setOnItemScraped(std::function<void(const ContentItem&)> callback) {
    onItemScraped_ = callback;
}

std::vector<ContentItem> RedditScraper::getLastScrapedItems() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    return lastScrapedItems_;
}

std::vector<ContentItem> RedditScraper::fetchPostComments(const std::string& subreddit, const std::string& postId) {
    std::vector<ContentItem> items;
    
    if (!rateLimiter_->acquire()) {
        Logger::warn("Rate limit exceeded for Reddit API");
        return items;
    }
    
    authenticate();
    
    if (accessToken_.empty()) {
        return items;
    }
    
    std::string url = "https://oauth.reddit.com/r/" + subreddit + "/comments/" + postId;
    std::map<std::string, std::string> headers = {
        {"User-Agent", userAgent_},
        {"Authorization", "Bearer " + accessToken_}
    };
    
    try {
        HttpResponse response = httpClient_->get(url, headers);
        
        if (response.statusCode == 200) {
            nlohmann::json responseJson = nlohmann::json::parse(response.body);
            
            if (responseJson.is_array() && responseJson.size() > 1) {
                parseCommentsRecursive(responseJson[1]["data"]["children"], items);
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Error fetching comments: " + std::string(e.what()));
    }
    
    return items;
}

void RedditScraper::parseCommentsRecursive(const nlohmann::json& children, std::vector<ContentItem>& items) {
    for (const auto& child : children) {
        if (child.contains("data")) {
            const auto& data = child["data"];
            
            if (data.contains("body")) {
                items.push_back(parseComment(data));
            }
            
            if (data.contains("replies") && !data["replies"].is_null()) {
                if (data["replies"].contains("data") && data["replies"]["data"].contains("children")) {
                    parseCommentsRecursive(data["replies"]["data"]["children"], items);
                }
            }
        }
    }
}

} // namespace ModAI
