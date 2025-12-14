#pragma once

#include "core/ContentItem.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace ModAI {

class RailguardOverlay : public QWidget {
    Q_OBJECT

private:
    QLabel* titleLabel_;
    QLabel* contentLabel_;
    QPushButton* reviewButton_;
    QPushButton* dismissButton_;
    
    ContentItem currentItem_;

public:
    explicit RailguardOverlay(QWidget* parent = nullptr);
    
    void showBlockedItem(const ContentItem& item);
    void hide();

private slots:
    void onReviewClicked();
    void onDismissClicked();

signals:
    void reviewRequested(const std::string& itemId);
    void overrideAction(const std::string& itemId, const std::string& newStatus);
};

} // namespace ModAI

