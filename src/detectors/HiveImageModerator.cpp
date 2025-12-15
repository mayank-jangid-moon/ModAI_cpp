#include "detectors/HiveImageModerator.h"
#include "network/HttpClient.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace ModAI {

HiveImageModerator::HiveImageModerator(std::unique_ptr<HttpClient> httpClient, 
                                       const std::string& apiKey)
    : httpClient_(std::move(httpClient))
    , apiKey_(apiKey)
    , rateLimiter_(std::make_unique<RateLimiter>(100, std::chrono::seconds(60))) {
    if (apiKey_.empty()) {
        Logger::warn("Hive API key is empty - image moderation will be skipped");
    }
}

VisualModerationResult HiveImageModerator::analyzeImage(const std::vector<uint8_t>& imageBytes, 
                                                         const std::string& mime) {
    VisualModerationResult result;
    
    // Skip if no API key
    if (apiKey_.empty()) {
        return result;
    }
    
    rateLimiter_->waitIfNeeded();
    
    // Hive visual moderation (v2 task sync)
    std::string url = "https://api.thehive.ai/api/v2/task/sync";
    
    HttpRequest req;
    req.url = url;
    req.method = "POST";
    // Hive expects "Token <key>"
    req.headers["Authorization"] = "Token " + apiKey_;
    req.contentType = "multipart/form-data";
    req.binaryData = imageBytes;
    
    try {
        HttpResponse response = httpClient_->post(req);
        
        if (!response.success) {
            Logger::error("Hive API error: " + response.errorMessage);
            return result;
        }
        
        if (response.statusCode != 200) {
            Logger::error("Hive API returned status: " + std::to_string(response.statusCode));
            return result;
        }
        
        auto json = nlohmann::json::parse(response.body);
        
        // v3 format: { "output": [ { "classes": [ { "class_name": "...", "value": 0.9 }, ... ] } ] }
        if (json.contains("output") && json["output"].is_array()) {
            for (const auto& item : json["output"]) {
                if (item.contains("classes") && item["classes"].is_array()) {
                    for (const auto& cls : item["classes"]) {
                        if (cls.contains("class_name") && cls.contains("value")) {
                            std::string label = cls["class_name"].get<std::string>();
                            double score = cls["value"].get<double>();
                            result.labels[label] = score;
                        }
                    }
                }
            }
        }

        // Legacy formats
        if (json.contains("status") && json["status"] == "completed") {
            if (json.contains("result") && json["result"].contains("output")) {
                auto output = json["result"]["output"];
                if (output.is_array()) {
                    for (const auto& item : output) {
                        if (item.contains("classes")) {
                            for (const auto& cls : item["classes"]) {
                                if (cls.contains("class") && cls.contains("score")) {
                                    std::string label = cls["class"].get<std::string>();
                                    double score = cls["score"].get<double>();
                                    result.labels[label] = score;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (json.contains("predictions") && json["predictions"].is_array()) {
            for (const auto& pred : json["predictions"]) {
                if (pred.contains("label") && pred.contains("confidence")) {
                    std::string label = pred["label"].get<std::string>();
                    double confidence = pred["confidence"].get<double>();
                    result.labels[label] = confidence;
                }
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("Exception in HiveImageModerator: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace ModAI

