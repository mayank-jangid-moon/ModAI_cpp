#include "export/Exporter.h"
#include "core/ContentItem.h"
#include "utils/Logger.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>

namespace ModAI {

void Exporter::exportToPDF(const std::vector<ContentItem>& items, const std::string& filepath) {
    Logger::warn("PDF export is not supported in headless mode.");
}

void Exporter::exportToCSV(const std::vector<ContentItem>& items, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        Logger::error("Failed to open file for CSV export: " + filepath);
        return;
    }
    
    // Header
    file << "id,timestamp,subreddit,author,content_type,ai_score,sexual,violence,hate,drugs,decision,rule_id\n";
    
    // Data rows
    for (const auto& item : items) {
        file << item.id << ","
             << item.timestamp << ","
             << item.subreddit << ","
             << (item.author.has_value() ? item.author.value() : "") << ","
             << item.content_type << ","
             << item.ai_detection.ai_score << ","
             << item.moderation.labels.sexual << ","
             << item.moderation.labels.violence << ","
             << item.moderation.labels.hate << ","
             << item.moderation.labels.drugs << ","
             << item.decision.auto_action << ","
             << item.decision.rule_id << "\n";
    }
    
    file.close();
}

void Exporter::exportToJSON(const std::vector<ContentItem>& items, const std::string& filepath) {
    nlohmann::json j;
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    j["export_timestamp"] = std::ctime(&time_t);
    j["total_items"] = items.size();
    
    nlohmann::json itemsArray = nlohmann::json::array();
    for (const auto& item : items) {
        itemsArray.push_back(nlohmann::json::parse(item.toJson()));
    }
    j["items"] = itemsArray;
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        Logger::error("Failed to open file for JSON export: " + filepath);
        return;
    }
    
    file << j.dump(2);
    file.close();
}

} // namespace ModAI

