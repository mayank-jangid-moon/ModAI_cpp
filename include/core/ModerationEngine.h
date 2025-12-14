#pragma once

#include "core/ContentItem.h"
#include "core/RuleEngine.h"
#include "detectors/TextDetector.h"
#include "detectors/ImageModerator.h"
#include "detectors/TextModerator.h"
#include "storage/Storage.h"
#include <memory>
#include <functional>

namespace ModAI {

class ModerationEngine {
private:
    std::unique_ptr<TextDetector> textDetector_;
    std::unique_ptr<ImageModerator> imageModerator_;
    std::unique_ptr<TextModerator> textModerator_;
    std::unique_ptr<RuleEngine> ruleEngine_;
    std::unique_ptr<Storage> storage_;
    
    std::function<void(const ContentItem&)> onItemProcessed_;

public:
    ModerationEngine(
        std::unique_ptr<TextDetector> textDetector,
        std::unique_ptr<ImageModerator> imageModerator,
        std::unique_ptr<TextModerator> textModerator,
        std::unique_ptr<RuleEngine> ruleEngine,
        std::unique_ptr<Storage> storage
    );
    
    void processItem(ContentItem item);
    void setOnItemProcessed(std::function<void(const ContentItem&)> callback);
};

} // namespace ModAI

