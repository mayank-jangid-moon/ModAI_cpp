#pragma once

#include <vector>
#include <map>
#include <string>
#include <cstdint>

namespace ModAI {

struct VisualModerationResult {
    std::map<std::string, double> labels;  // label -> confidence
};

class ImageModerator {
public:
    virtual ~ImageModerator() = default;
    virtual VisualModerationResult analyzeImage(const std::vector<uint8_t>& imageBytes, 
                                                const std::string& mime) = 0;
};

} // namespace ModAI
