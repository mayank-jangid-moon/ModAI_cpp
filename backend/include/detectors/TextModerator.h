#pragma once

#include <string>
#include <map>
#include <vector>

namespace ModAI {

struct TextModerationResult {
    std::vector<std::pair<std::string, double>> labels;  // label, confidence pairs
};

class TextModerator {
public:
    virtual ~TextModerator() = default;
    virtual TextModerationResult analyzeText(const std::string& text) = 0;
};

} // namespace ModAI
