#include "core/ContentItem.h"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <random>

using json = nlohmann::json;

namespace ModAI {

std::string generateUUID() {
    // Simple UUID v4 generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }
    return ss.str();
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return ss.str();
}

ContentItem::ContentItem() 
    : id(generateUUID())
    , timestamp(getCurrentTimestamp())
    , source("reddit")
    , content_type("text")
    , schema_version(1) {
}

ContentItem::ContentItem(const std::string& subreddit, const std::string& content_type)
    : ContentItem() {
    this->subreddit = subreddit;
    this->content_type = content_type;
}

std::string ContentItem::toJson() const {
    json j;
    j["id"] = id;
    j["timestamp"] = timestamp;
    j["source"] = source;
    j["subreddit"] = subreddit;
    if (author.has_value()) {
        j["author"] = author.value();
    } else {
        j["author"] = nullptr;
    }
    j["content_type"] = content_type;
    if (text.has_value()) {
        j["text"] = text.value();
    } else {
        j["text"] = nullptr;
    }
    if (image_path.has_value()) {
        j["image_path"] = image_path.value();
    } else {
        j["image_path"] = nullptr;
    }
    
    // AI detection
    j["ai_detection"]["model"] = ai_detection.model;
    j["ai_detection"]["ai_score"] = ai_detection.ai_score;
    j["ai_detection"]["label"] = ai_detection.label;
    j["ai_detection"]["confidence"] = ai_detection.confidence;
    
    // Moderation
    j["moderation"]["provider"] = moderation.provider;
    j["moderation"]["labels"]["sexual"] = moderation.labels.sexual;
    j["moderation"]["labels"]["violence"] = moderation.labels.violence;
    j["moderation"]["labels"]["hate"] = moderation.labels.hate;
    j["moderation"]["labels"]["drugs"] = moderation.labels.drugs;
    for (const auto& [key, value] : moderation.labels.additional_labels) {
        j["moderation"]["labels"][key] = value;
    }
    
    // Decision
    j["decision"]["auto_action"] = decision.auto_action;
    j["decision"]["rule_id"] = decision.rule_id;
    j["decision"]["threshold_triggered"] = decision.threshold_triggered;
    
    j["schema_version"] = schema_version;
    
    return j.dump();
}

ContentItem ContentItem::fromJson(const std::string& jsonStr) {
    ContentItem item;
    json j = json::parse(jsonStr);
    
    item.id = j.value("id", "");
    item.timestamp = j.value("timestamp", "");
    item.source = j.value("source", "reddit");
    item.subreddit = j.value("subreddit", "");
    if (j.contains("author") && !j["author"].is_null()) {
        item.author = j["author"].get<std::string>();
    }
    item.content_type = j.value("content_type", "text");
    if (j.contains("text") && !j["text"].is_null()) {
        item.text = j["text"].get<std::string>();
    }
    if (j.contains("image_path") && !j["image_path"].is_null()) {
        item.image_path = j["image_path"].get<std::string>();
    }
    
    // AI detection
    if (j.contains("ai_detection")) {
        item.ai_detection.model = j["ai_detection"].value("model", "");
        item.ai_detection.ai_score = j["ai_detection"].value("ai_score", 0.0);
        item.ai_detection.label = j["ai_detection"].value("label", "");
        item.ai_detection.confidence = j["ai_detection"].value("confidence", 0.0);
    }
    
    // Moderation
    if (j.contains("moderation")) {
        item.moderation.provider = j["moderation"].value("provider", "");
        if (j["moderation"].contains("labels")) {
            item.moderation.labels.sexual = j["moderation"]["labels"].value("sexual", 0.0);
            item.moderation.labels.violence = j["moderation"]["labels"].value("violence", 0.0);
            item.moderation.labels.hate = j["moderation"]["labels"].value("hate", 0.0);
            item.moderation.labels.drugs = j["moderation"]["labels"].value("drugs", 0.0);
            for (auto& [key, value] : j["moderation"]["labels"].items()) {
                if (key != "sexual" && key != "violence" && key != "hate" && key != "drugs") {
                    if (value.is_number()) {
                        item.moderation.labels.additional_labels[key] = value.get<double>();
                    }
                }
            }
        }
    }
    
    // Decision
    if (j.contains("decision")) {
        item.decision.auto_action = j["decision"].value("auto_action", "allow");
        item.decision.rule_id = j["decision"].value("rule_id", "");
        item.decision.threshold_triggered = j["decision"].value("threshold_triggered", false);
    }
    
    item.schema_version = j.value("schema_version", 1);
    
    return item;
}

} // namespace ModAI
