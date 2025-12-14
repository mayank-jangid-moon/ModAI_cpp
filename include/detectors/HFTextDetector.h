#pragma once

#include "detectors/TextDetector.h"
#include "network/HttpClient.h"
#include "network/RateLimiter.h"
#include <string>
#include <memory>

namespace ModAI {

class HFTextDetector : public TextDetector {
private:
    std::unique_ptr<HttpClient> httpClient_;
    std::string apiToken_;
    std::string modelId_;
    std::unique_ptr<RateLimiter> rateLimiter_;

public:
    HFTextDetector(std::unique_ptr<HttpClient> httpClient, 
                   const std::string& apiToken);
    
    TextDetectResult analyze(const std::string& text) override;
};

} // namespace ModAI

