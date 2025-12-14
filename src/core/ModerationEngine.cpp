#include "core/ModerationEngine.h"
#include "utils/Logger.h"
#include <QFile>
#include <QImage>
#include <QBuffer>
#include <QIODevice>

namespace ModAI {

ModerationEngine::ModerationEngine(
    std::unique_ptr<TextDetector> textDetector,
    std::unique_ptr<ImageModerator> imageModerator,
    std::unique_ptr<TextModerator> textModerator,
    std::unique_ptr<RuleEngine> ruleEngine,
    std::unique_ptr<Storage> storage)
    : textDetector_(std::move(textDetector))
    , imageModerator_(std::move(imageModerator))
    , textModerator_(std::move(textModerator))
    , ruleEngine_(std::move(ruleEngine))
    , storage_(std::move(storage)) {
}

void ModerationEngine::processItem(ContentItem item) {
    Logger::info("Processing content item: " + item.id);
    
    // Run AI text detection if text content exists
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
            QFile file(QString::fromStdString(item.image_path.value()));
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray imageData = file.readAll();
                std::vector<uint8_t> imageBytes(imageData.begin(), imageData.end());
                
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
            }
        } catch (const std::exception& e) {
            Logger::error("Failed to process image: " + std::string(e.what()));
        }
    }
    
    // Evaluate rules
    auto matchingRules = ruleEngine_->getMatchingRules(item);
    if (!matchingRules.empty()) {
        item.decision.auto_action = matchingRules[0].action;
        item.decision.rule_id = matchingRules[0].id;
        item.decision.threshold_triggered = true;
    } else {
        item.decision.auto_action = "allow";
        item.decision.threshold_triggered = false;
    }
    
    // Save to storage
    try {
        storage_->saveContent(item);
    } catch (const std::exception& e) {
        Logger::error("Failed to save content item: " + std::string(e.what()));
    }
    
    // Notify callback
    if (onItemProcessed_) {
        onItemProcessed_(item);
    }
}

void ModerationEngine::setOnItemProcessed(std::function<void(const ContentItem&)> callback) {
    onItemProcessed_ = callback;
}

} // namespace ModAI

