#include "core/ResultCache.h"
#include "utils/Logger.h"
#include <fstream>
#include <iostream>
#include <ctime>

namespace ModAI {

ResultCache::ResultCache(const std::string& filePath) : filePath_(filePath) {
    load();
}

void ResultCache::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(filePath_);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        try {
            auto j = nlohmann::json::parse(line);
            if (j.contains("hash") && j.contains("result")) {
                cache_[j["hash"]] = j["result"];
            }
        } catch (...) {}
    }
}

std::optional<nlohmann::json> ResultCache::get(const std::string& hash) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(hash);
    if (it != cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ResultCache::put(const std::string& hash, const nlohmann::json& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cache_.find(hash) != cache_.end()) return;
    
    cache_[hash] = result;
    append(hash, result);
}

void ResultCache::append(const std::string& hash, const nlohmann::json& result) {
    std::ofstream file(filePath_, std::ios::app);
    if (file.is_open()) {
        nlohmann::json entry;
        entry["hash"] = hash;
        entry["result"] = result;
        entry["timestamp"] = std::time(nullptr);
        file << entry.dump() << "\n";
    }
}

}
