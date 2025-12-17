#pragma once
#include <string>
#include <map>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>

namespace ModAI {

class ResultCache {
public:
    explicit ResultCache(const std::string& filePath);
    
    std::optional<nlohmann::json> get(const std::string& hash);
    void put(const std::string& hash, const nlohmann::json& result);
    
private:
    std::string filePath_;
    std::map<std::string, nlohmann::json> cache_;
    std::mutex mutex_;
    
    void load();
    void append(const std::string& hash, const nlohmann::json& result);
};

}
