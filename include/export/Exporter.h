#pragma once

#include "core/ContentItem.h"
#include <vector>
#include <string>

namespace ModAI {

class Exporter {
public:
    static void exportToPDF(const std::vector<ContentItem>& items, const std::string& filepath);
    static void exportToCSV(const std::vector<ContentItem>& items, const std::string& filepath);
    static void exportToJSON(const std::vector<ContentItem>& items, const std::string& filepath);
};

} // namespace ModAI

