#pragma once

#include "detectors/ImageModerator.h"
#include "network/HttpClient.h"
#include "network/RateLimiter.h"
#include <string>
#include <memory>

namespace ModAI {

class HiveImageModerator : public ImageModerator {
private:
    std::unique_ptr<HttpClient> httpClient_;
    std::string apiKey_;
    std::unique_ptr<RateLimiter> rateLimiter_;

public:
    HiveImageModerator(std::unique_ptr<HttpClient> httpClient, 
                       const std::string& apiKey);
    
    VisualModerationResult analyzeImage(const std::vector<uint8_t>& imageBytes, 
                                        const std::string& mime) override;
};

} // namespace ModAI

