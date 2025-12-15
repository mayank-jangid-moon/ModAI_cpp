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
    , modelId_("openai-community/roberta-large-openai-detector")
    , rateLimiter_(std::make_unique<RateLimiter>(30, std::chrono::seconds(60))) {
    if (apiToken_.empty()) {
        Logger::warn("HuggingFace API token is empty - text detection will be skipped");
    } else {
        Logger::info("HuggingFace detector initialized with model: " + modelId_);
    }
}

TextDetectResult HFTextDetector::analyze(const std::string& text) {
    TextDetectResult result;
    result.label = "unknown";
    result.ai_score = 0.0;
    result.confidence = 0.0;
    
    // Skip if no API token
    if (apiToken_.empty()) {
        return result;
    }
    
    rateLimiter_->waitIfNeeded();
    
    // Use router hf-inference endpoint (api-inference is deprecated)
    std::string url = "https://router.huggingface.co/hf-inference/models/" + modelId_;
    
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
        
        // Normalized parsing for common HF formats (array of scores or single object)
        auto consumeScores = [&](const nlohmann::json& arr) {
            double aiProb = 0.0;
            double humanProb = 0.0;
            for (const auto& obj : arr) {
                if (!obj.is_object()) continue;
                if (!obj.contains("label") || !obj.contains("score")) continue;
                std::string lbl = obj["label"].get<std::string>();
                double score = obj["score"].get<double>();
                // Map known labels
                if (lbl == "LABEL_1" || lbl == "ai_generated" || lbl == "fake") {
                    aiProb = std::max(aiProb, score);
                } else if (lbl == "LABEL_0" || lbl == "human" || lbl == "real") {
                    humanProb = std::max(humanProb, score);
                }
            }
            // Normalize decision
            // Apply guardrails: require AI prob to be meaningfully higher and above a floor
            const double aiFloor = 0.70;      // minimum AI probability
            const double margin  = 0.15;      // AI must exceed human by this margin
            bool confidentAI = (aiProb >= aiFloor) && ((aiProb - humanProb) >= margin);

            if (confidentAI) {
                result.label = "ai_generated";
                result.ai_score = aiProb;
                result.confidence = aiProb;
            } else {
                result.label = "human";
                result.ai_score = aiProb;       // keep raw AI prob for downstream rules
                result.confidence = humanProb;  // human confidence dominates
            }
        };

        if (json.is_array()) {
            if (!json.empty() && json[0].is_array()) {
                consumeScores(json[0]);
            } else {
                consumeScores(json);
            }
        } else if (json.is_object()) {
            nlohmann::json arr = nlohmann::json::array();
            arr.push_back(json);
            consumeScores(arr);
        }
        
    } catch (const std::exception& e) {
        Logger::error("Exception in HFTextDetector: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace ModAI

