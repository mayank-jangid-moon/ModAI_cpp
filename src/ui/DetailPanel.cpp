#include "ui/DetailPanel.h"
#include <QHBoxLayout>
#include <QString>
#include <QPixmap>

namespace ModAI {

DetailPanel::DetailPanel(QWidget* parent)
    : QWidget(parent) {
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    titleLabel_ = new QLabel("Select an item to view details");
    titleLabel_->setWordWrap(true);
    layout->addWidget(titleLabel_);
    
    contentText_ = new QTextEdit;
    contentText_->setReadOnly(true);
    layout->addWidget(contentText_);
    
    imageLabel_ = new QLabel;
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setMaximumHeight(200);
    imageLabel_->hide();
    layout->addWidget(imageLabel_);
    
    aiScoreLabel_ = new QLabel;
    layout->addWidget(aiScoreLabel_);
    
    moderationLabel_ = new QLabel;
    layout->addWidget(moderationLabel_);
    
    decisionLabel_ = new QLabel;
    layout->addWidget(decisionLabel_);
    
    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    blockButton_ = new QPushButton("Block");
    allowButton_ = new QPushButton("Allow");
    reviewButton_ = new QPushButton("Mark for Review");
    
    buttonLayout->addWidget(blockButton_);
    buttonLayout->addWidget(allowButton_);
    buttonLayout->addWidget(reviewButton_);
    
    layout->addLayout(buttonLayout);
    layout->addStretch();
    
    connect(blockButton_, &QPushButton::clicked, this, &DetailPanel::onBlockClicked);
    connect(allowButton_, &QPushButton::clicked, this, &DetailPanel::onAllowClicked);
    connect(reviewButton_, &QPushButton::clicked, this, &DetailPanel::onReviewClicked);
}

void DetailPanel::setContentItem(const ContentItem& item) {
    currentItem_ = item;
    
    titleLabel_->setText(QString::fromStdString("Item: " + item.id));
    
    if (item.text.has_value()) {
        contentText_->setPlainText(QString::fromStdString(item.text.value()));
    } else {
        contentText_->setPlainText("No text content");
    }
    
    if (item.image_path.has_value()) {
        // Load and display image
        QString imagePath = QString::fromStdString(item.image_path.value());
        QPixmap pixmap;
        if (imagePath.startsWith("http://") || imagePath.startsWith("https://")) {
            // TODO: Download image from URL
            // For now, skip remote images
        } else {
            pixmap.load(imagePath);
        }
        if (!pixmap.isNull()) {
            imageLabel_->setPixmap(pixmap.scaled(400, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            imageLabel_->show();
        }
    } else {
        imageLabel_->hide();
    }
    
    aiScoreLabel_->setText(QString("AI Score: %1 (Label: %2, Confidence: %3)")
                          .arg(item.ai_detection.ai_score, 0, 'f', 2)
                          .arg(QString::fromStdString(item.ai_detection.label))
                          .arg(item.ai_detection.confidence, 0, 'f', 2));
    
    QString modText = "Moderation Labels: ";
    if (item.moderation.labels.sexual > 0.0) {
        modText += QString("sexual: %1 ").arg(item.moderation.labels.sexual, 0, 'f', 2);
    }
    if (item.moderation.labels.violence > 0.0) {
        modText += QString("violence: %1 ").arg(item.moderation.labels.violence, 0, 'f', 2);
    }
    if (item.moderation.labels.hate > 0.0) {
        modText += QString("hate: %1 ").arg(item.moderation.labels.hate, 0, 'f', 2);
    }
    if (item.moderation.labels.drugs > 0.0) {
        modText += QString("drugs: %1 ").arg(item.moderation.labels.drugs, 0, 'f', 2);
    }
    moderationLabel_->setText(modText);
    
    decisionLabel_->setText(QString("Decision: %1 (Rule: %2)")
                           .arg(QString::fromStdString(item.decision.auto_action))
                           .arg(QString::fromStdString(item.decision.rule_id)));
}

void DetailPanel::clear() {
    currentItem_ = ContentItem();
    titleLabel_->setText("Select an item to view details");
    contentText_->clear();
    imageLabel_->hide();
    aiScoreLabel_->clear();
    moderationLabel_->clear();
    decisionLabel_->clear();
}

void DetailPanel::onBlockClicked() {
    if (!currentItem_.id.empty()) {
        emit actionRequested(currentItem_.id, "block");
    }
}

void DetailPanel::onAllowClicked() {
    if (!currentItem_.id.empty()) {
        emit actionRequested(currentItem_.id, "allow");
    }
}

void DetailPanel::onReviewClicked() {
    if (!currentItem_.id.empty()) {
        emit actionRequested(currentItem_.id, "review");
    }
}

} // namespace ModAI

