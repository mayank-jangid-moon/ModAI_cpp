#pragma once

#include "core/ContentItem.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>

namespace ModAI {

class DetailPanel : public QWidget {
    Q_OBJECT

private:
    QLabel* titleLabel_;
    QTextEdit* contentText_;
    QLabel* imageLabel_;
    QLabel* aiScoreLabel_;
    QLabel* moderationLabel_;
    QLabel* decisionLabel_;
    
    QPushButton* blockButton_;
    QPushButton* allowButton_;
    QPushButton* reviewButton_;
    
    ContentItem currentItem_;

public:
    explicit DetailPanel(QWidget* parent = nullptr);
    
    void setContentItem(const ContentItem& item);
    void clear();

private slots:
    void onBlockClicked();
    void onAllowClicked();
    void onReviewClicked();

signals:
    void actionRequested(const std::string& itemId, const std::string& action);
};

} // namespace ModAI

