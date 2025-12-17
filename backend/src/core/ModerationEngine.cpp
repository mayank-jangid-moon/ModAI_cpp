#include "core/ModerationEngine.h"
#include "utils/Logger.h"
#include <fstream>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>

namespace ModAI {

ModerationEngine::ModerationEngine(
    std::unique_ptr<TextDetector> textDetector,
    std::unique_ptr<ImageModerator> imageModerator,
    std::unique_ptr<TextModerator> textModerator,
    std::unique_ptr<RuleEngine> ruleEngine,
    std::unique_ptr<Storage> storage,
    std::unique_ptr<ResultCache> cache)
    : textDetector_(std::move(textDetector))
    , imageModerator_(std::move(imageModerator))
    , textModerator_(std::move(textModerator))
    , ruleEngine_(std::move(ruleEngine))
    , storage_(std::move(storage))
    , cache_(std::move(cache)) {
}

std::string ModerationEngine::computeImageHash(const std::vector<uint8_t>& imageData) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(imageData.data(), imageData.size(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

void ModerationEngine::processItem(ContentItem item) {
    Logger::info("Processing content item: " + item.id);
    
    // Run AI text detection if text content exists (always fresh, no caching)
    if (item.content_type == "text" && item.text.has_value()) {
        auto textResult = textDetector_->analyze(item.text.value());
        item.ai_detection.model = "desklib/ai-text-detector-v1.01";
        item.ai_detection.ai_score = textResult.ai_score;
        item.ai_detection.label = textResult.label;
        item.ai_detection.confidence = textResult.confidence;
        
        // Also run text moderation
        auto textModResult = textModerator_->analyzeText(item.text.value());
        for (const auto& [label, confidence] : textModResult.labels) {
            if (label == "sexual" || label == "violence" || label == "hate" || label == "drugs") {
                if (label == "sexual") item.moderation.labels.sexual = confidence;
                else if (label == "violence") item.moderation.labels.violence = confidence;
                else if (label == "hate") item.moderation.labels.hate = confidence;
                else if (label == "drugs") item.moderation.labels.drugs = confidence;
            } else {
                item.moderation.labels.additional_labels[label] = confidence;
            }
        }
        item.moderation.provider = "hive";
    }
    
    // Run image moderation if image content exists
    if (item.content_type == "image" && item.image_path.has_value()) {
        try {
            std::ifstream file(item.image_path.value(), std::ios::binary);
            if (file.is_open()) {
                // Read file into vector
                file.seekg(0, std::ios::end);
                size_t fileSize = file.tellg();
                file.seekg(0, std::ios::beg);
                
                std::vector<uint8_t> imageBytes(fileSize);
                file.read(reinterpret_cast<char*>(imageBytes.data()), fileSize);
                file.close();
                
                std::string hashStr = computeImageHash(imageBytes);
                
                auto cached = cache_->get(hashStr);
                if (cached) {
                    auto j = cached.value();
                    if (j.contains("moderation")) {
                        auto mod = j["moderation"];
                        item.moderation.provider = mod.value("provider", "hive");
                        if (mod.contains("labels")) {
                            auto labels = mod["labels"];
                            item.moderation.labels.sexual = labels.value("sexual", 0.0);
                            item.moderation.labels.violence = labels.value("violence", 0.0);
                            item.moderation.labels.hate = labels.value("hate", 0.0);
                            item.moderation.labels.drugs = labels.value("drugs", 0.0);
                        }
                    }
                } else {
                    auto imageResult = imageModerator_->analyzeImage(imageBytes, "image/jpeg");
                    for (const auto& [label, confidence] : imageResult.labels) {
                        if (label == "sexual" || label == "violence" || label == "hate" || label == "drugs") {
                            if (label == "sexual") item.moderation.labels.sexual = confidence;
                            else if (label == "violence") item.moderation.labels.violence = confidence;
                            else if (label == "hate") item.moderation.labels.hate = confidence;
                            else if (label == "drugs") item.moderation.labels.drugs = confidence;
                        } else {
                            item.moderation.labels.additional_labels[label] = confidence;
                        }
                    }
                    item.moderation.provider = "hive";
                    
                    // Cache result
                    nlohmann::json result;
                    nlohmann::json mod;
                    mod["provider"] = item.moderation.provider;
                    nlohmann::json labels;
                    labels["sexual"] = item.moderation.labels.sexual;
                    labels["violence"] = item.moderation.labels.violence;
                    labels["hate"] = item.moderation.labels.hate;
                    labels["drugs"] = item.moderation.labels.drugs;
                    mod["labels"] = labels;
                    result["moderation"] = mod;
                    cache_->put(hashStr, result);
                }
            } else {
                Logger::error("Failed to open image file: " + item.image_path.value());
            }
        } catch (const std::exception& e) {
            Logger::error("Exception processing image: " + std::string(e.what()));
        }
    }
    
    // Apply rule engine
    std::string action = ruleEngine_->evaluate(item);
    item.decision.auto_action = action;
    
    auto matchingRules = ruleEngine_->getMatchingRules(item);
    if (!matchingRules.empty()) {
        item.decision.rule_id = matchingRules[0].id;
        item.decision.threshold_triggered = true;
    }
    
    Logger::info("Decision: " + action + " (rule: " + item.decision.rule_id + ")");
    
    // Save to storage
    try {
        storage_->saveContent(item);
    } catch (const std::exception& e) {
        Logger::error("Failed to save content: " + std::string(e.what()));
    }
    
    // Call callback if set
    if (onItemProcessed_) {
        onItemProcessed_(item);
    }
}

void ModerationEngine::setOnItemProcessed(std::function<void(const ContentItem&)> callback) {
    onItemProcessed_ = callback;
}

} // namespace ModAI
