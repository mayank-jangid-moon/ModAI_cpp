#pragma once

#include "core/ContentItem.h"
#include <string>
#include <vector>
#include <map>

namespace ModAI {

struct Rule {
    std::string id;
    std::string name;
    std::string condition;  // e.g., "ai_score > 0.8", "sexual > 0.9"
    std::string action;     // "allow", "block", "review"
    std::string subreddit;  // empty = global
    bool enabled = true;
};

class RuleEngine {
private:
    std::vector<Rule> rules_;
    
    bool evaluateCondition(const Rule& rule, const ContentItem& item);
    double getValue(const std::string& field, const ContentItem& item);

public:
    void loadRulesFromJson(const std::string& jsonPath);
    void addRule(const Rule& rule);
    void clearRules();
    
    std::string evaluate(const ContentItem& item);
    std::vector<Rule> getMatchingRules(const ContentItem& item);
};

} // namespace ModAI
