#pragma once

#include <string>
#include <map>
#include <chrono>
#include <optional>

namespace ModAI {

struct AIDetection {
    std::string model;
    double ai_score = 0.0;
    std::string label;  // "ai_generated" or "human"
    double confidence = 0.0;
};

struct ModerationLabels {
    double sexual = 0.0;
    double violence = 0.0;
    double hate = 0.0;
    double drugs = 0.0;
    double harassment = 0.0;
    double self_harm = 0.0;
    double illicit = 0.0;
    std::map<std::string, double> additional_labels;
};

struct ModerationResult {
    std::string provider;
    ModerationLabels labels;
};

struct Decision {
    std::string auto_action;  // "allow", "block", "review"
    std::string rule_id;
    bool threshold_triggered = false;
    std::string human_decision;
    std::string human_reviewer;
    std::string human_notes;
    long human_review_timestamp = 0;
};

class ContentItem {
public:
    std::string id;
    std::string timestamp;  // ISO-8601
    std::string source;
    std::string subreddit;
    std::optional<std::string> author;
    std::string content_type;  // "text" or "image"
    std::optional<std::string> text;
    std::optional<std::string> text_content;  // Alias for text
    std::optional<std::string> image_path;
    std::optional<std::string> post_id;  // Reddit post ID for fetching comments
    std::optional<std::string> content_id;  // Generic content ID
    std::map<std::string, std::string> metadata;  // Additional metadata
    
    AIDetection ai_detection;
    ModerationResult moderation;
    Decision decision;
    
    int schema_version = 1;
    
    ContentItem();
    ContentItem(const std::string& subreddit, const std::string& content_type);
    
    std::string toJson() const;
    static ContentItem fromJson(const std::string& json);
};

} // namespace ModAI
