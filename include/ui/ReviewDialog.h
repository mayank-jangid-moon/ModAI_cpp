#pragma once

#include "core/ContentItem.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace ModAI {

class ReviewDialog : public QDialog {
    Q_OBJECT

private:
    ContentItem item_;
    QLabel* contentLabel_;
    QLabel* aiScoreLabel_;
    QLabel* moderationLabel_;
    QPushButton* allowButton_;
    QPushButton* blockButton_;

public:
    explicit ReviewDialog(const ContentItem& item, QWidget* parent = nullptr);
    
signals:
    void actionTaken(const std::string& itemId, const std::string& action);
};

} // namespace ModAI

