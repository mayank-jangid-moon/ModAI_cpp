#include "export/Exporter.h"
#include "core/ContentItem.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace ModAI {

void Exporter::exportToCSV(const std::vector<ContentItem>& items, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for CSV export");
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
    nlohmann::json j = nlohmann::json::array();
    
    for (const auto& item : items) {
        j.push_back(nlohmann::json::parse(item.toJson()));
    }
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for JSON export");
    }
    
    file << j.dump(2);  // Pretty print with 2-space indentation
    file.close();
}

} // namespace ModAI
