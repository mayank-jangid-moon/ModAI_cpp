#include "detectors/HFTextDetector.h"
#include "network/HttpClient.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace ModAI {

HFTextDetector::HFTextDetector(std::unique_ptr<HttpClient> httpClient, 
                               const std::string& apiToken)
    : httpClient_(std::move(httpClient))
    , apiToken_(apiToken)
    , modelId_("desklib/ai-text-detector-v1.01")
    , rateLimiter_(std::make_unique<RateLimiter>(30, std::chrono::seconds(60))) {
}

TextDetectResult HFTextDetector::analyze(const std::string& text) {
    TextDetectResult result;
    
    rateLimiter_->waitIfNeeded();
    
    std::string url = "https://api-inference.huggingface.co/models/" + modelId_;
    
    HttpRequest req;
    req.url = url;
    req.method = "POST";
    req.headers["Authorization"] = "Bearer " + apiToken_;
    req.headers["Content-Type"] = "application/json";
    
    nlohmann::json payload;
    payload["inputs"] = text;
    req.body = payload.dump();
    
    try {
        HttpResponse response = httpClient_->post(req);
        
        if (!response.success) {
            Logger::error("HF API error: " + response.errorMessage);
            return result;
        }
        
        if (response.statusCode != 200) {
            Logger::error("HF API returned status: " + std::to_string(response.statusCode));
            return result;
        }
        
        auto json = nlohmann::json::parse(response.body);
        
        // Handle different response formats
        if (json.is_array() && !json.empty()) {
            json = json[0];
        }
        
        if (json.contains("label")) {
            result.label = json["label"].get<std::string>();
        }
        
        if (json.contains("score")) {
            result.confidence = json["score"].get<double>();
            // If label is "ai_generated", ai_score = confidence, else 1 - confidence
            if (result.label == "ai_generated") {
                result.ai_score = result.confidence;
            } else {
                result.ai_score = 1.0 - result.confidence;
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("Exception in HFTextDetector: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace ModAI

