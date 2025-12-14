#include "core/RuleEngine.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace ModAI {

void RuleEngine::loadRulesFromJson(const std::string& jsonPath) {
    rules_.clear();
    
    try {
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            Logger::warn("Could not open rules file: " + jsonPath);
            return;
        }
        
        nlohmann::json j;
        file >> j;
        
        if (j.contains("rules") && j["rules"].is_array()) {
            for (const auto& ruleJson : j["rules"]) {
                Rule rule;
                rule.id = ruleJson.value("id", "");
                rule.name = ruleJson.value("name", "");
                rule.condition = ruleJson.value("condition", "");
                rule.action = ruleJson.value("action", "allow");
                rule.subreddit = ruleJson.value("subreddit", "");
                rule.enabled = ruleJson.value("enabled", true);
                
                if (!rule.id.empty() && !rule.condition.empty()) {
                    rules_.push_back(rule);
                }
            }
        }
        
        Logger::info("Loaded " + std::to_string(rules_.size()) + " rules from " + jsonPath);
    } catch (const std::exception& e) {
        Logger::error("Failed to load rules: " + std::string(e.what()));
    }
}

void RuleEngine::addRule(const Rule& rule) {
    rules_.push_back(rule);
}

void RuleEngine::clearRules() {
    rules_.clear();
}

double RuleEngine::getValue(const std::string& field, const ContentItem& item) {
    if (field == "ai_score") {
        return item.ai_detection.ai_score;
    } else if (field == "sexual") {
        return item.moderation.labels.sexual;
    } else if (field == "violence") {
        return item.moderation.labels.violence;
    } else if (field == "hate") {
        return item.moderation.labels.hate;
    } else if (field == "drugs") {
        return item.moderation.labels.drugs;
    } else if (item.moderation.labels.additional_labels.count(field) > 0) {
        return item.moderation.labels.additional_labels.at(field);
    }
    return 0.0;
}

bool RuleEngine::evaluateCondition(const Rule& rule, const ContentItem& item) {
    // Simple condition parser: "field > value" or "field < value" or "field >= value"
    std::regex pattern(R"((\w+)\s*(>=|<=|>|<|==)\s*([\d.]+))");
    std::smatch match;
    
    if (std::regex_search(rule.condition, match, pattern)) {
        std::string field = match[1].str();
        std::string op = match[2].str();
        double threshold = std::stod(match[3].str());
        
        double value = getValue(field, item);
        
        if (op == ">") {
            return value > threshold;
        } else if (op == ">=") {
            return value >= threshold;
        } else if (op == "<") {
            return value < threshold;
        } else if (op == "<=") {
            return value <= threshold;
        } else if (op == "==") {
            return std::abs(value - threshold) < 0.0001;
        }
    }
    
    return false;
}

std::string RuleEngine::evaluate(const ContentItem& item) {
    // Check rules in order, return first matching action
    for (const auto& rule : rules_) {
        if (!rule.enabled) {
            continue;
        }
        
        // Check subreddit match
        if (!rule.subreddit.empty() && rule.subreddit != item.subreddit) {
            continue;
        }
        
        if (evaluateCondition(rule, item)) {
            return rule.action;
        }
    }
    
    return "allow";  // Default action
}

std::vector<Rule> RuleEngine::getMatchingRules(const ContentItem& item) {
    std::vector<Rule> matching;
    
    for (const auto& rule : rules_) {
        if (!rule.enabled) {
            continue;
        }
        
        if (!rule.subreddit.empty() && rule.subreddit != item.subreddit) {
            continue;
        }
        
        if (evaluateCondition(rule, item)) {
            matching.push_back(rule);
        }
    }
    
    return matching;
}

} // namespace ModAI

