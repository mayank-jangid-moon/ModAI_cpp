#pragma once

#include "storage/Storage.h"
#include <mutex>
#include <string>
#include <fstream>

namespace ModAI {

class JsonlStorage : public Storage {
private:
    mutable std::mutex writeMutex_;
    std::string basePath_;
    std::string contentFile_;
    std::string actionFile_;
    
    void ensureDirectoryExists(const std::string& path);
    void appendLine(const std::string& filepath, const std::string& line);
    std::vector<std::string> readLines(const std::string& filepath);

public:
    explicit JsonlStorage(const std::string& basePath);
    
    void saveContent(const ContentItem& item) override;
    void saveAction(const HumanAction& action) override;
    
    std::vector<ContentItem> loadAllContent() override;
    std::vector<HumanAction> loadAllActions() override;
};

} // namespace ModAI

