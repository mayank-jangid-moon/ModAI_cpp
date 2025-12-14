#include "ui/ReviewDialog.h"
#include <QHBoxLayout>
#include <QString>

namespace ModAI {

ReviewDialog::ReviewDialog(const ContentItem& item, QWidget* parent)
    : QDialog(parent)
    , item_(item) {
    
    setWindowTitle("Review Content Item");
    setMinimumSize(500, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    contentLabel_ = new QLabel;
    if (item.text.has_value()) {
        contentLabel_->setText(QString::fromStdString(item.text.value()));
    } else {
        contentLabel_->setText("No text content");
    }
    contentLabel_->setWordWrap(true);
    layout->addWidget(contentLabel_);
    
    aiScoreLabel_ = new QLabel;
    aiScoreLabel_->setText(QString("AI Score: %1").arg(item.ai_detection.ai_score, 0, 'f', 2));
    layout->addWidget(aiScoreLabel_);
    
    moderationLabel_ = new QLabel;
    QString modText = "Moderation: ";
    if (item.moderation.labels.sexual > 0.0) modText += QString("sexual: %1 ").arg(item.moderation.labels.sexual, 0, 'f', 2);
    if (item.moderation.labels.violence > 0.0) modText += QString("violence: %1 ").arg(item.moderation.labels.violence, 0, 'f', 2);
    moderationLabel_->setText(modText);
    layout->addWidget(moderationLabel_);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    allowButton_ = new QPushButton("Allow");
    blockButton_ = new QPushButton("Block");
    
    buttonLayout->addWidget(allowButton_);
    buttonLayout->addWidget(blockButton_);
    layout->addLayout(buttonLayout);
    
    connect(allowButton_, &QPushButton::clicked, [this]() {
        emit actionTaken(item_.id, "allow");
        accept();
    });
    
    connect(blockButton_, &QPushButton::clicked, [this]() {
        emit actionTaken(item_.id, "block");
        accept();
    });
}

} // namespace ModAI

