#pragma once

#include "detectors/TextModerator.h"
#include "network/HttpClient.h"
#include "network/RateLimiter.h"
#include <string>
#include <memory>

namespace ModAI {

class HiveTextModerator : public TextModerator {
private:
    std::unique_ptr<HttpClient> httpClient_;
    std::string apiKey_;
    std::unique_ptr<RateLimiter> rateLimiter_;

public:
    HiveTextModerator(std::unique_ptr<HttpClient> httpClient, 
                      const std::string& apiKey);
    
    TextModerationResult analyzeText(const std::string& text) override;
};

} // namespace ModAI

