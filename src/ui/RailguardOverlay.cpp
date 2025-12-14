#include "ui/RailguardOverlay.h"
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QString>
#include <QColor>

namespace ModAI {

RailguardOverlay::RailguardOverlay(QWidget* parent)
    : QWidget(parent) {
    
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    
    QWidget* container = new QWidget(this);
    container->setStyleSheet(
        "background-color: #ff4444;"
        "border-radius: 10px;"
        "padding: 20px;"
        "color: white;"
        "font-weight: bold;"
    );
    
    QVBoxLayout* layout = new QVBoxLayout(container);
    
    titleLabel_ = new QLabel("⚠️ AUTO-BLOCKED CONTENT");
    titleLabel_->setStyleSheet("font-size: 18px; font-weight: bold;");
    layout->addWidget(titleLabel_);
    
    contentLabel_ = new QLabel;
    contentLabel_->setWordWrap(true);
    layout->addWidget(contentLabel_);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    reviewButton_ = new QPushButton("Review (R)");
    dismissButton_ = new QPushButton("Dismiss (D)");
    
    reviewButton_->setStyleSheet("background-color: white; color: #ff4444; padding: 10px; border-radius: 5px;");
    dismissButton_->setStyleSheet("background-color: rgba(255,255,255,0.3); color: white; padding: 10px; border-radius: 5px;");
    
    buttonLayout->addWidget(reviewButton_);
    buttonLayout->addWidget(dismissButton_);
    layout->addLayout(buttonLayout);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(container);
    
    connect(reviewButton_, &QPushButton::clicked, this, &RailguardOverlay::onReviewClicked);
    connect(dismissButton_, &QPushButton::clicked, this, &RailguardOverlay::onDismissClicked);
    
    // Add shadow effect
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 5);
    container->setGraphicsEffect(shadow);
}

void RailguardOverlay::showBlockedItem(const ContentItem& item) {
    currentItem_ = item;
    
    QString content = "Subreddit: " + QString::fromStdString(item.subreddit) + "\n";
    if (item.text.has_value()) {
        QString text = QString::fromStdString(item.text.value());
        content += "Content: " + text.left(100) + (text.length() > 100 ? "..." : "");
    }
    content += "\nReason: " + QString::fromStdString(item.decision.rule_id);
    
    contentLabel_->setText(content);
    
    // Position overlay in top-right corner
    if (parentWidget()) {
        QPoint pos = parentWidget()->mapToGlobal(QPoint(0, 0));
        move(pos.x() + parentWidget()->width() - 400, pos.y() + 20);
        resize(380, 200);
    }
    
    QWidget::show();
    raise();
    activateWindow();
}

void RailguardOverlay::hide() {
    QWidget::hide();
}

void RailguardOverlay::onReviewClicked() {
    if (!currentItem_.id.empty()) {
        emit reviewRequested(currentItem_.id);
        hide();
    }
}

void RailguardOverlay::onDismissClicked() {
    hide();
}

} // namespace ModAI

