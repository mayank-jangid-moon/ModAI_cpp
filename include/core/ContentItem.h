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
    std::optional<std::string> image_path;
    
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

// Q_DECLARE_METATYPE removed for core purity. 
// If Qt frontend needs it, it should be declared in a Qt-specific header or wrapper.

