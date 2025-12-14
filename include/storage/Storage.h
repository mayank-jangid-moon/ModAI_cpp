#pragma once

#include "core/ContentItem.h"
#include <vector>
#include <string>

namespace ModAI {

struct HumanAction {
    std::string action_id;
    std::string content_id;
    std::string timestamp;
    std::string reviewer;
    std::string previous_status;
    std::string new_status;
    std::string reason;
    std::optional<std::string> notes;
    int schema_version = 1;
    
    std::string toJson() const;
    static HumanAction fromJson(const std::string& json);
};

class Storage {
public:
    virtual ~Storage() = default;
    
    virtual void saveContent(const ContentItem& item) = 0;
    virtual void saveAction(const HumanAction& action) = 0;
    
    virtual std::vector<ContentItem> loadAllContent() = 0;
    virtual std::vector<HumanAction> loadAllActions() = 0;
};

} // namespace ModAI

