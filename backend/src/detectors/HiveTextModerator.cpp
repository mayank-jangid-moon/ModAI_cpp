#include "detectors/HiveTextModerator.h"
#include "network/HttpClient.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace ModAI {

HiveTextModerator::HiveTextModerator(std::unique_ptr<HttpClient> httpClient, 
                                     const std::string& apiKey)
    : httpClient_(std::move(httpClient))
    , apiKey_(apiKey)
    , rateLimiter_(std::make_unique<RateLimiter>(100, std::chrono::seconds(60))) {
    if (apiKey_.empty()) {
        Logger::warn("Hive API key is empty - text moderation will be skipped");
    }
}

TextModerationResult HiveTextModerator::analyzeText(const std::string& text) {
    TextModerationResult result;
    
    // Skip if no API key
    if (apiKey_.empty()) {
        return result;
    }
    
    rateLimiter_->waitIfNeeded();
    
    // Hive text moderation v3 API
    std::string url = "https://api.thehive.ai/api/v3/hive/text-moderation";
    
    // Hive API has 1024 character limit
    std::string processedText = text;
    if (processedText.length() > 1024) {
        processedText = processedText.substr(0, 1024);
        Logger::debug("Truncated text from " + std::to_string(text.length()) + " to 1024 chars for Hive API");
    }
    
    HttpRequest req;
    req.url = url;
    req.method = "POST";
    req.headers["Authorization"] = "Bearer " + apiKey_;
    req.headers["Content-Type"] = "application/json";
    
    nlohmann::json payload;
    payload["input"] = nlohmann::json::array();
    payload["input"][0]["text"] = processedText;
    req.body = payload.dump();
    
    try {
        HttpResponse response = httpClient_->post(req);
        
        if (response.statusCode != 200 && response.statusCode != 0) {
            Logger::error("Hive Text API returned status: " + std::to_string(response.statusCode));
            Logger::error("Response body: " + response.body);
            return result;
        }
        
        if (!response.success && response.body.empty()) {
            Logger::error("Hive Text API error: " + response.errorMessage);
            return result;
        }
        
        auto json = nlohmann::json::parse(response.body);
        
        // Parse v3 API response format
        if (json.contains("output") && json["output"].is_array() && !json["output"].empty()) {
            auto& output = json["output"][0];
            
            if (output.contains("classes") && output["classes"].is_array()) {
                for (const auto& classObj : output["classes"]) {
                    if (classObj.contains("class") && classObj.contains("value")) {
                        std::string className = classObj["class"].get<std::string>();
                        int value = classObj["value"].get<int>();
                        
                        // Convert 0-3 scale to 0.0-1.0 confidence
                        double confidence = value / 3.0;
                        
                        if (confidence > 0.0) {
                            result.labels.push_back({className, confidence});
                            Logger::debug("Hive label: " + className + " = " + std::to_string(confidence) + 
                                        " (raw value: " + std::to_string(value) + ")");
                        }
                    }
                }
            }
        }
        
        Logger::debug("Hive moderation found " + std::to_string(result.labels.size()) + " labels");
        
    } catch (const std::exception& e) {
        Logger::error("Exception in HiveTextModerator: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace ModAI
