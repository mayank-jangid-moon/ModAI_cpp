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
    
    // Hive Visual Moderation API v3
    std::string url = "https://api.thehive.ai/api/v3/hive/visual-moderation";
    
    HttpRequest req;
    req.url = url;
    req.method = "POST";
    // V3 API uses Bearer token authorization
    req.headers["Authorization"] = "Bearer " + apiKey_;
    req.contentType = "multipart/form-data";
    req.binaryData = imageBytes;
    
    try {
        HttpResponse response = httpClient_->post(req);
        
        if (!response.success) {
            Logger::error("Hive Visual Moderation API error: " + response.errorMessage);
            return result;
        }
        
        if (response.statusCode != 200) {
            Logger::error("Hive Visual Moderation API returned status: " + std::to_string(response.statusCode) + 
                         ", Body: " + response.body);
            return result;
        }
        
        auto json = nlohmann::json::parse(response.body);
        
        // Parse V3 Visual Moderation API response format
        // Expected: { "task_id": "...", "model": "hive/visual-moderation", "version": "1",
        //            "output": [ { "classes": [ { "class_name": "general_nsfw", "value": 0.99 }, ... ] } ] }
        if (json.contains("output") && json["output"].is_array() && !json["output"].empty()) {
            const auto& firstOutput = json["output"][0];
            
            if (firstOutput.contains("classes") && firstOutput["classes"].is_array()) {
                for (const auto& cls : firstOutput["classes"]) {
                    if (cls.contains("class_name") && cls.contains("value")) {
                        std::string className = cls["class_name"].get<std::string>();
                        double value = cls["value"].get<double>();
                        result.labels[className] = value;
                    }
                }
                
                Logger::debug("Hive Visual Moderation: Found " + std::to_string(result.labels.size()) + " classifications");
            }
        } else {
            Logger::warn("Hive Visual Moderation: Unexpected response format - no output array found");
        }
        
    } catch (const nlohmann::json::parse_error& e) {
        Logger::error("JSON parse error in HiveImageModerator: " + std::string(e.what()));
    } catch (const std::exception& e) {
        Logger::error("Exception in HiveImageModerator: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace ModAI

