#include "export/Exporter.h"
#include "core/ContentItem.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>

namespace ModAI {

void Exporter::exportToPDF(const std::vector<ContentItem>& items, const std::string& filepath) {
    // For now, create a simple text-based report
    // In production, use Qt's QPrinter or libharu
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for PDF export");
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    file << "Trust & Safety Report\n";
    file << "====================\n\n";
    file << "Generated: " << std::ctime(&time_t);
    file << "Total Items: " << items.size() << "\n\n";
    
    int blockedCount = 0;
    int reviewedCount = 0;
    
    for (const auto& item : items) {
        if (item.decision.auto_action == "block") blockedCount++;
        if (item.decision.auto_action == "review") reviewedCount++;
        
        file << "Item ID: " << item.id << "\n";
        file << "Subreddit: " << item.subreddit << "\n";
        file << "AI Score: " << item.ai_detection.ai_score << "\n";
        file << "Decision: " << item.decision.auto_action << "\n";
        if (item.text.has_value()) {
            file << "Text: " << item.text.value().substr(0, 200) << "...\n";
        }
        file << "---\n";
    }
    
    file << "\nSummary:\n";
    file << "Blocked: " << blockedCount << "\n";
    file << "Reviewed: " << reviewedCount << "\n";
    
    file.close();
}

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
        throw std::runtime_error("Failed to open file for JSON export");
    }
    
    file << j.dump(2);
    file.close();
}

} // namespace ModAI

