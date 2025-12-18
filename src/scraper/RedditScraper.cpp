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
    , imageModerator_(nullptr)
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

void RedditScraper::setImageModerator(std::unique_ptr<ImageModerator> imageModerator) {
    imageModerator_ = std::move(imageModerator);
    Logger::info("Image moderator set for Reddit scraper");
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
    
    // Extract Reddit post ID (e.g., "1po4bx2" from "t3_1po4bx2")
    if (postJson.contains("name")) {
        std::string fullId = postJson["name"].get<std::string>();
        // Remove "t3_" prefix if present
        if (fullId.find("t3_") == 0) {
            item.post_id = fullId.substr(3);
        } else {
            item.post_id = fullId;
        }
    }
    
    // Also store it as the item ID
    if (postJson.contains("id")) {
        item.id = postJson["id"].get<std::string>();
    }
    
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
            url.find(".webp") != std::string::npos ||
            url.find("i.redd.it") != std::string::npos ||
            url.find("i.imgur.com") != std::string::npos) {
            item.content_type = "image";
            
            // Store title and selftext as text for text moderation
            std::string combinedText;
            if (postJson.contains("title")) {
                std::string title = postJson["title"].get<std::string>();
                if (!title.empty()) {
                    combinedText = title;
                }
            }
            
            // Also include selftext body if present
            if (postJson.contains("selftext")) {
                std::string selftext = postJson["selftext"].get<std::string>();
                if (!selftext.empty() && selftext != "[deleted]" && selftext != "[removed]") {
                    if (!combinedText.empty()) {
                        combinedText += "\n\n" + selftext;
                    } else {
                        combinedText = selftext;
                    }
                }
            }
            
            if (!combinedText.empty()) {
                item.text = combinedText;
                Logger::info("Image post has text: " + combinedText.substr(0, 50));
            }
            
            // Download image
            std::string localPath = downloadImage(url);
            if (!localPath.empty()) {
                item.image_path = localPath;
                // Image moderation will be handled by ModerationEngine after queuing
            } else {
                item.image_path = url;
                Logger::warn("Failed to download image from " + url + ", cannot moderate");
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

std::vector<ContentItem> RedditScraper::fetchPostComments(const std::string& subreddit, const std::string& postId) {
    std::vector<ContentItem> items;
    
    rateLimiter_->waitIfNeeded();
    authenticate();
    
    const bool useOAuth = !accessToken_.empty();
    std::string url = useOAuth
        ? "https://oauth.reddit.com/r/" + subreddit + "/comments/" + postId + ".json"
        : "https://www.reddit.com/r/" + subreddit + "/comments/" + postId + ".json";
    
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    req.headers["User-Agent"] = userAgent_;
    if (useOAuth) {
        req.headers["Authorization"] = "Bearer " + accessToken_;
    }
    
    try {
        Logger::info("Fetching comments from URL: " + url);
        HttpResponse response = httpClient_->get(url, req.headers);
        
        if (response.success && response.statusCode == 200) {
            auto json = nlohmann::json::parse(response.body);
            
            // Reddit returns an array with 2 elements: [0]=post, [1]=comments
            if (json.is_array() && json.size() >= 2) {
                auto& commentsListing = json[1];
                
                if (commentsListing.contains("data") && commentsListing["data"].contains("children")) {
                    parseCommentsRecursive(commentsListing["data"]["children"], items);
                    Logger::info("Fetched " + std::to_string(items.size()) + " comments for post " + postId);
                }
            }
        } else {
            Logger::warn("Failed to fetch comments for post " + postId + ": HTTP " + std::to_string(response.statusCode));
        }
    } catch (const std::exception& e) {
        Logger::error("Exception fetching post comments: " + std::string(e.what()));
    }
    
    return items;
}

void RedditScraper::parseCommentsRecursive(const nlohmann::json& children, std::vector<ContentItem>& items) {
    for (const auto& child : children) {
        if (child.contains("data")) {
            auto& data = child["data"];
            
            // Skip "more" comments markers
            if (child.value("kind", "") == "more") {
                continue;
            }
            
            ContentItem item = parseComment(data);
            items.push_back(item);
            
            // Recursively parse replies
            if (data.contains("replies") && data["replies"].is_object()) {
                if (data["replies"].contains("data") && data["replies"]["data"].contains("children")) {
                    parseCommentsRecursive(data["replies"]["data"]["children"], items);
                }
            }
        }
    }
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

void RedditScraper::moderateImage(ContentItem& item) {
    if (!imageModerator_) {
        Logger::warn("Image moderator not set, skipping image moderation for " + item.id);
        return;
    }
    
    if (!item.image_path.has_value() || item.image_path->empty()) {
        Logger::warn("No image path for item " + item.id);
        return;
    }
    
    try {
        // Read image file into bytes
        QFile imageFile(QString::fromStdString(*item.image_path));
        if (!imageFile.open(QIODevice::ReadOnly)) {
            Logger::error("Failed to open image file: " + *item.image_path);
            return;
        }
        
        QByteArray imageData = imageFile.readAll();
        imageFile.close();
        
        // Convert to vector<uint8_t>
        std::vector<uint8_t> imageBytes(imageData.begin(), imageData.end());
        
        // Determine MIME type from file extension
        std::string mimeType = "image/jpeg";
        if (item.image_path->find(".png") != std::string::npos) {
            mimeType = "image/png";
        } else if (item.image_path->find(".webp") != std::string::npos) {
            mimeType = "image/webp";
        }
        
        Logger::info("Moderating image: " + *item.image_path + " (size: " + std::to_string(imageBytes.size()) + " bytes)");
        
        // Analyze image
        auto result = imageModerator_->analyzeImage(imageBytes, mimeType);
        
        // Store moderation results
        item.moderation.provider = "hive_visual";
        
        // Map Hive categories to our moderation labels
        for (const auto& [category, score] : result.labels) {
            // NSFW categories
            if (category == "general_nsfw" || 
                category.find("yes_female_nudity") == 0 ||
                category.find("yes_male_nudity") == 0 ||
                category.find("yes_sexual_activity") == 0 ||
                category.find("yes_sexual_intent") == 0) {
                item.moderation.labels.sexual = std::max(item.moderation.labels.sexual, score);
            }
            // Violence categories
            else if (category.find("gun_") == 0 ||
                     category.find("knife_") == 0 ||
                     category.find("bloody") != std::string::npos ||
                     category == "yes_fight" ||
                     category.find("corpse") != std::string::npos) {
                item.moderation.labels.violence = std::max(item.moderation.labels.violence, score);
            }
            // Hate categories
            else if (category == "yes_nazi" ||
                     category == "yes_kkk" ||
                     category == "yes_terrorist" ||
                     category == "yes_confederate") {
                item.moderation.labels.hate = std::max(item.moderation.labels.hate, score);
            }
            // Drugs categories
            else if (category == "yes_pills" ||
                     category == "yes_smoking" ||
                     category == "yes_marijuana" ||
                     category.find("injectables") != std::string::npos) {
                item.moderation.labels.drugs = std::max(item.moderation.labels.drugs, score);
            }
            
            // Store all labels in additional_labels
            item.moderation.labels.additional_labels[category] = score;
        }
        
        // Determine auto action based on thresholds
        const double BLOCK_THRESHOLD = 0.7;
        const double REVIEW_THRESHOLD = 0.5;
        
        double maxScore = std::max({
            item.moderation.labels.sexual,
            item.moderation.labels.violence,
            item.moderation.labels.hate,
            item.moderation.labels.drugs
        });
        
        if (maxScore >= BLOCK_THRESHOLD) {
            item.decision.auto_action = "block";
            item.decision.threshold_triggered = true;
            Logger::info("Image blocked: " + item.id + " (score: " + std::to_string(maxScore) + ")");
        } else if (maxScore >= REVIEW_THRESHOLD) {
            item.decision.auto_action = "review";
            item.decision.threshold_triggered = true;
            Logger::info("Image flagged for review: " + item.id + " (score: " + std::to_string(maxScore) + ")");
        } else {
            item.decision.auto_action = "allow";
            Logger::info("Image allowed: " + item.id + " (score: " + std::to_string(maxScore) + ")");
        }
        
    } catch (const std::exception& e) {
        Logger::error("Exception moderating image: " + std::string(e.what()));
    }
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

