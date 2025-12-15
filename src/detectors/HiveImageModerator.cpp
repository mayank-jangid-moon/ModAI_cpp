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
}

VisualModerationResult HiveImageModerator::analyzeImage(const std::vector<uint8_t>& imageBytes, 
                                                         const std::string& mime) {
    VisualModerationResult result;
    
    rateLimiter_->waitIfNeeded();
    
    std::string url = "https://api.thehive.ai/api/v2/task/sync";
    
    HttpRequest req;
    req.url = url;
    req.method = "POST";
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
        
        // Parse Hive response format
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
        
        // Alternative format: direct predictions array
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

