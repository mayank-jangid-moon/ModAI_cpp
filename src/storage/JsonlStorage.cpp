#include "storage/JsonlStorage.h"
#include "core/ContentItem.h"
#include "utils/Logger.h"
#include "storage/Storage.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace ModAI {

std::string HumanAction::toJson() const {
    nlohmann::json j;
    j["action_id"] = action_id;
    j["content_id"] = content_id;
    j["timestamp"] = timestamp;
    j["reviewer"] = reviewer;
    j["previous_status"] = previous_status;
    j["new_status"] = new_status;
    j["reason"] = reason;
    if (notes.has_value()) {
        j["notes"] = notes.value();
    } else {
        j["notes"] = nullptr;
    }
    j["schema_version"] = schema_version;
    return j.dump();
}

HumanAction HumanAction::fromJson(const std::string& jsonStr) {
    HumanAction action;
    nlohmann::json j = nlohmann::json::parse(jsonStr);
    
    action.action_id = j.value("action_id", "");
    action.content_id = j.value("content_id", "");
    action.timestamp = j.value("timestamp", "");
    action.reviewer = j.value("reviewer", "");
    action.previous_status = j.value("previous_status", "");
    action.new_status = j.value("new_status", "");
    action.reason = j.value("reason", "");
    if (j.contains("notes") && !j["notes"].is_null()) {
        action.notes = j["notes"].get<std::string>();
    }
    action.schema_version = j.value("schema_version", 1);
    
    return action;
}

JsonlStorage::JsonlStorage(const std::string& basePath) 
    : basePath_(basePath) {
    contentFile_ = basePath + "/content.jsonl";
    actionFile_ = basePath + "/actions.jsonl";
    
    ensureDirectoryExists(basePath);
    ensureDirectoryExists(basePath + "/cache");
    ensureDirectoryExists(basePath + "/logs");
    ensureDirectoryExists(basePath + "/exports/reports");
    ensureDirectoryExists(basePath + "/exports/csv");
}

void JsonlStorage::ensureDirectoryExists(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            fs::create_directories(path);
        }
    } catch (const std::exception& e) {
        Logger::error("Failed to create directory: " + path + " - " + e.what());
    }
}

void JsonlStorage::appendLine(const std::string& filepath, const std::string& line) {
    std::lock_guard<std::mutex> lock(writeMutex_);
    
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }
    
    file << line << "\n";
    file.flush();
    file.close();
}

std::vector<std::string> JsonlStorage::readLines(const std::string& filepath) {
    std::vector<std::string> lines;
    
    if (!fs::exists(filepath)) {
        return lines;
    }
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::warn("Failed to open file for reading: " + filepath);
        return lines;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    
    return lines;
}

void JsonlStorage::saveContent(const ContentItem& item) {
    try {
        std::string json = item.toJson();
        appendLine(contentFile_, json);
    } catch (const std::exception& e) {
        Logger::error("Failed to save content item: " + std::string(e.what()));
        throw;
    }
}

void JsonlStorage::saveAction(const HumanAction& action) {
    try {
        std::string json = action.toJson();
        appendLine(actionFile_, json);
    } catch (const std::exception& e) {
        Logger::error("Failed to save action: " + std::string(e.what()));
        throw;
    }
}

std::vector<ContentItem> JsonlStorage::loadAllContent() {
    std::vector<ContentItem> items;
    auto lines = readLines(contentFile_);
    
    for (const auto& line : lines) {
        try {
            items.push_back(ContentItem::fromJson(line));
        } catch (const std::exception& e) {
            Logger::warn("Skipping corrupt line in content.jsonl: " + std::string(e.what()));
        }
    }
    
    return items;
}

std::vector<HumanAction> JsonlStorage::loadAllActions() {
    std::vector<HumanAction> actions;
    auto lines = readLines(actionFile_);
    
    for (const auto& line : lines) {
        try {
            actions.push_back(HumanAction::fromJson(line));
        } catch (const std::exception& e) {
            Logger::warn("Skipping corrupt line in actions.jsonl: " + std::string(e.what()));
        }
    }
    
    return actions;
}

} // namespace ModAI

