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
}

TextModerationResult HiveTextModerator::analyzeText(const std::string& text) {
    TextModerationResult result;
    
    rateLimiter_->waitIfNeeded();
    
    std::string url = "https://api.thehive.ai/api/v2/task/sync";
    
    HttpRequest req;
    req.url = url;
    req.method = "POST";
    req.headers["Authorization"] = "Token " + apiKey_;
    req.headers["Content-Type"] = "application/json";
    
    nlohmann::json payload;
    payload["text_data"] = text;
    req.body = payload.dump();
    
    try {
        HttpResponse response = httpClient_->post(req);
        
        if (!response.success) {
            Logger::error("Hive Text API error: " + response.errorMessage);
            return result;
        }
        
        if (response.statusCode != 200) {
            Logger::error("Hive Text API returned status: " + std::to_string(response.statusCode));
            return result;
        }
        
        auto json = nlohmann::json::parse(response.body);
        
        // Parse labels array
        if (json.contains("labels") && json["labels"].is_array()) {
            for (const auto& labelObj : json["labels"]) {
                if (labelObj.contains("label") && labelObj.contains("confidence")) {
                    std::string label = labelObj["label"].get<std::string>();
                    double confidence = labelObj["confidence"].get<double>();
                    result.labels.push_back({label, confidence});
                }
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("Exception in HiveTextModerator: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace ModAI

