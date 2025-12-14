#pragma once

#include <string>

namespace ModAI {

struct TextDetectResult {
    double ai_score = 0.0;
    std::string label;  // "ai_generated" or "human"
    double confidence = 0.0;
};

class TextDetector {
public:
    virtual ~TextDetector() = default;
    virtual TextDetectResult analyze(const std::string& text) = 0;
};

} // namespace ModAI

