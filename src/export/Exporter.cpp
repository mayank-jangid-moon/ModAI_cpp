#include "export/Exporter.h"
#include "core/ContentItem.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextTable>
#include <QTextTableFormat>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QImage>
#include <QFile>

namespace ModAI {

void Exporter::exportToPDF(const std::vector<ContentItem>& items, const std::string& filepath) {
    QPdfWriter writer(QString::fromStdString(filepath));
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(300); // 300 DPI
    
    QPainter painter(&writer);
    
    // Use QTextDocument for rich text layout
    QTextDocument doc;
    QTextCursor cursor(&doc);
    
    // Title
    QTextCharFormat titleFormat;
    titleFormat.setFontPointSize(24);
    titleFormat.setFontWeight(QFont::Bold);
    cursor.insertText("Trust & Safety Report\n", titleFormat);
    
    // Date
    QTextCharFormat normalFormat;
    normalFormat.setFontPointSize(12);
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    cursor.insertText("Generated: " + QString::fromStdString(std::ctime(&time_t)), normalFormat);
    cursor.insertBlock();
    
    // Summary
    int blockedCount = 0;
    int reviewedCount = 0;
    for (const auto& item : items) {
        if (item.decision.auto_action == "block") blockedCount++;
        if (item.decision.auto_action == "review") reviewedCount++;
    }
    
    cursor.insertText(QString("Total Items: %1\n").arg(items.size()), normalFormat);
    cursor.insertText(QString("Blocked: %1\n").arg(blockedCount), normalFormat);
    cursor.insertText(QString("Reviewed: %1\n\n").arg(reviewedCount), normalFormat);
    
    // Table
    QTextTableFormat tableFormat;
    tableFormat.setHeaderRowCount(1);
    tableFormat.setBorder(1);
    tableFormat.setCellPadding(5);
    tableFormat.setCellSpacing(0);
    tableFormat.setWidth(QTextLength(QTextLength::PercentageLength, 100));
    
    QTextTable* table = cursor.insertTable(items.size() + 1, 5, tableFormat);
    
    // Headers
    QTextCharFormat headerFormat = normalFormat;
    headerFormat.setFontWeight(QFont::Bold);
    
    table->cellAt(0, 0).firstCursorPosition().insertText("ID", headerFormat);
    table->cellAt(0, 1).firstCursorPosition().insertText("Subreddit", headerFormat);
    table->cellAt(0, 2).firstCursorPosition().insertText("Type", headerFormat);
    table->cellAt(0, 3).firstCursorPosition().insertText("AI Score", headerFormat);
    table->cellAt(0, 4).firstCursorPosition().insertText("Status", headerFormat);
    
    // Rows
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        int row = i + 1;
        
        table->cellAt(row, 0).firstCursorPosition().insertText(QString::fromStdString(item.id).left(8), normalFormat);
        table->cellAt(row, 1).firstCursorPosition().insertText(QString::fromStdString(item.subreddit), normalFormat);
        table->cellAt(row, 2).firstCursorPosition().insertText(QString::fromStdString(item.content_type), normalFormat);
        table->cellAt(row, 3).firstCursorPosition().insertText(QString::number(item.ai_detection.ai_score, 'f', 2), normalFormat);
        table->cellAt(row, 4).firstCursorPosition().insertText(QString::fromStdString(item.decision.auto_action), normalFormat);
    }
    
    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
    cursor.insertText("\nDetailed Items:\n", titleFormat);
    
    // Details
    for (const auto& item : items) {
        cursor.insertText("ID: " + QString::fromStdString(item.id), normalFormat);
        cursor.insertBlock();
        
        if (item.text.has_value()) {
            cursor.insertText("Text: " + QString::fromStdString(item.text.value()).left(500), normalFormat);
            cursor.insertBlock();
        }
        
        if (item.image_path.has_value()) {
            QString imgPath = QString::fromStdString(item.image_path.value());
            if (QFile::exists(imgPath)) {
                QImage img(imgPath);
                if (!img.isNull()) {
                    // Scale image to fit
                    img = img.scaled(400, 300, Qt::KeepAspectRatio);
                    cursor.insertImage(img);
                    cursor.insertBlock();
                }
            }
        }
        
        cursor.insertText("----------------------------------------\n", normalFormat);
    }
    
    doc.drawContents(&painter);
}

void Exporter::exportToCSV(const std::vector<ContentItem>& items, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for CSV export");
    }
    
    // Header
    file << "id,timestamp,subreddit,author,content_type,ai_score,sexual,violence,hate,drugs,decision,rule_id\n";
    
    // Data rows
    for (const auto& item : items) {
        file << item.id << ","
             << item.timestamp << ","
             << item.subreddit << ","
             << (item.author.has_value() ? item.author.value() : "") << ","
             << item.content_type << ","
             << item.ai_detection.ai_score << ","
             << item.moderation.labels.sexual << ","
             << item.moderation.labels.violence << ","
             << item.moderation.labels.hate << ","
             << item.moderation.labels.drugs << ","
             << item.decision.auto_action << ","
             << item.decision.rule_id << "\n";
    }
    
    file.close();
}

void Exporter::exportToJSON(const std::vector<ContentItem>& items, const std::string& filepath) {
    nlohmann::json j;
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    j["export_timestamp"] = std::ctime(&time_t);
    j["total_items"] = items.size();
    
    nlohmann::json itemsArray = nlohmann::json::array();
    for (const auto& item : items) {
        itemsArray.push_back(nlohmann::json::parse(item.toJson()));
    }
    j["items"] = itemsArray;
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for JSON export");
    }
    
    file << j.dump(2);
    file.close();
}

} // namespace ModAI

