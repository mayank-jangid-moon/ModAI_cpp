#include "ui/AITextDetectorPanel.h"
#include "utils/Logger.h"
#include <QFont>
#include <QPalette>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QFutureWatcher>
#include "detectors/LocalAIDetector.h"
#include "ui/ChatbotPanel.h"

namespace ModAI {

AITextDetectorPanel::AITextDetectorPanel(QWidget* parent)
    : QWidget(parent)
    , textInput_(nullptr)
    , analyzeButton_(nullptr)
    , clearButton_(nullptr)
    , resultLabel_(nullptr)
    , scoreBar_(nullptr)
    , detailsLabel_(nullptr)
    , statusLabel_(nullptr)
    , loadingTimer_(nullptr)
    , loadingDots_(0) {
    setupUI();
}

void AITextDetectorPanel::initialize(std::shared_ptr<TextDetector> textDetector) {
    textDetector_ = textDetector;
    
    // Check if it's a LocalAIDetector with isAvailable method
    auto localDetector = std::dynamic_pointer_cast<LocalAIDetector>(textDetector_);
    if (localDetector && localDetector->isAvailable()) {
        statusLabel_->setText("AI detector ready");
        analyzeButton_->setEnabled(true);
    } else {
        statusLabel_->setText("AI detector not available");
        analyzeButton_->setEnabled(false);
    }
}

void AITextDetectorPanel::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    
    // Header - Simple title
    auto* headerLabel = new QLabel("AI Text Detector");
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(14);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    headerLabel->setStyleSheet("color: #2c3e50; padding: 4px;");
    layout->addWidget(headerLabel);
    
    // Main content area - horizontal split
    auto* contentWidget = new QWidget;
    auto* contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(16);
    
    // Left side - Text input
    auto* inputFrame = new QFrame;
    inputFrame->setStyleSheet(
        "QFrame { background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 8px; }"
    );
    auto* inputLayout = new QVBoxLayout(inputFrame);
    inputLayout->setContentsMargins(12, 12, 12, 12);
    inputLayout->setSpacing(8);
    
    auto* inputLabel = new QLabel("Text to analyze:");
    inputLabel->setStyleSheet("font-weight: bold; color: #495057; font-size: 18px;");
    inputLayout->addWidget(inputLabel);
    
    textInput_ = new QTextEdit;
    textInput_->setPlaceholderText("Paste your text here...\n\nMinimum recommended: 50 words for accurate detection.");
    textInput_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    textInput_->setStyleSheet(
        "QTextEdit { "
        "  border: 2px solid #ced4da; "
        "  border-radius: 6px; "
        "  padding: 10px; "
        "  font-size: 18px; "
        "  background-color: white; "
        "}"
        "QTextEdit:focus { "
        "  border-color: #4a90e2; "
        "}"
    );
    inputLayout->addWidget(textInput_, 1);
    
    contentLayout->addWidget(inputFrame, 3);
    
    // Right side - Results panel
    auto* resultsFrame = new QFrame;
    resultsFrame->setStyleSheet(
        "QFrame { background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 8px; }"
    );
    resultsFrame->setMinimumWidth(320);
    resultsFrame->setMaximumWidth(450);
    auto* resultsLayout = new QVBoxLayout(resultsFrame);
    resultsLayout->setContentsMargins(16, 16, 16, 16);
    resultsLayout->setSpacing(12);
    
    auto* resultsTitle = new QLabel("Results");
    resultsTitle->setStyleSheet("font-weight: bold; color: #495057; font-size: 18px; padding-bottom: 6px;");
    resultsLayout->addWidget(resultsTitle);
    
    // Result verdict
    resultLabel_ = new QLabel("Awaiting analysis...");
    resultLabel_->setStyleSheet(
        "background-color: white; border: 1px solid #dee2e6; border-radius: 6px; "
        "padding: 14px; font-size: 13pt; font-weight: bold; color: #495057;"
    );
    resultLabel_->setAlignment(Qt::AlignCenter);
    resultLabel_->setMinimumHeight(55);
    resultsLayout->addWidget(resultLabel_);
    
    // Score bar
    scoreBar_ = new QProgressBar;
    scoreBar_->setRange(0, 100);
    scoreBar_->setValue(0);
    scoreBar_->setTextVisible(true);
    scoreBar_->setFormat("AI Score: %p%");
    scoreBar_->setMinimumHeight(35);
    scoreBar_->setStyleSheet(
        "QProgressBar { border: 1px solid #dee2e6; border-radius: 4px; text-align: center; font-weight: bold; font-size: 16px; background-color: white; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #28a745, stop:0.5 #ffc107, stop:1 #dc3545); border-radius: 3px; }"
    );
    resultsLayout->addWidget(scoreBar_);
    
    // Details label
    detailsLabel_ = new QLabel("");
    detailsLabel_->setWordWrap(true);
    detailsLabel_->setStyleSheet(
        "color: #495057; font-size: 16px; padding: 10px; "
        "background-color: white; border: 1px solid #dee2e6; border-radius: 4px;"
    );
    detailsLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    resultsLayout->addWidget(detailsLabel_, 1);
    
    contentLayout->addWidget(resultsFrame, 2);
    
    layout->addWidget(contentWidget, 1);
    
    // Controls - modern styled buttons matching Reddit scraper page
    auto* controlsWidget = new QWidget;
    auto* buttonLayout = new QHBoxLayout(controlsWidget);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(12);
    
    analyzeButton_ = new QPushButton("Analyze Text");
    analyzeButton_->setMinimumHeight(40);
    analyzeButton_->setMinimumWidth(150);
    analyzeButton_->setCursor(Qt::PointingHandCursor);
    analyzeButton_->setStyleSheet(
        "QPushButton { "
        "  padding: 10px 24px; "
        "  background-color: #4a90e2; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 8px; "
        "  font-size: 14px; "
        "  font-weight: 600; "
        "}"
        "QPushButton:hover { "
        "  background-color: #357abd; "
        "}"
        "QPushButton:pressed { "
        "  background-color: #2868a8; "
        "}"
        "QPushButton:disabled { "
        "  background-color: #b0b0b0; "
        "  color: #e0e0e0; "
        "}"
    );
    
    clearButton_ = new QPushButton("Clear");
    clearButton_->setMinimumHeight(40);
    clearButton_->setMinimumWidth(100);
    clearButton_->setCursor(Qt::PointingHandCursor);
    clearButton_->setStyleSheet(
        "QPushButton { "
        "  padding: 10px 24px; "
        "  background-color: #6c757d; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 8px; "
        "  font-size: 14px; "
        "  font-weight: 600; "
        "}"
        "QPushButton:hover { "
        "  background-color: #5a6268; "
        "}"
        "QPushButton:pressed { "
        "  background-color: #495057; "
        "}"
    );
    
    buttonLayout->addWidget(analyzeButton_);
    buttonLayout->addWidget(clearButton_);
    buttonLayout->addStretch();
    
    layout->addWidget(controlsWidget);
    
    // Status label
    statusLabel_ = new QLabel("Ready");
    statusLabel_->setStyleSheet("color: #6c757d; font-size: 11px; padding: 2px;");
    layout->addWidget(statusLabel_);
    
    // Connections
    connect(analyzeButton_, &QPushButton::clicked, this, &AITextDetectorPanel::onAnalyzeText);
    connect(clearButton_, &QPushButton::clicked, this, &AITextDetectorPanel::onClearText);
}

void AITextDetectorPanel::onAnalyzeText() {
    QString text = textInput_->toPlainText();
    
    // Store original text for watermark check after analysis
    currentText_ = text;
    
    text = text.trimmed();
    if (text.isEmpty()) {
        statusLabel_->setText("Please enter some text to analyze");
        return;
    }
    
    auto localDetector = std::dynamic_pointer_cast<LocalAIDetector>(textDetector_);
    if (!localDetector || !localDetector->isAvailable()) {
        statusLabel_->setText("AI detector not available");
        return;
    }
    
    // Check minimum length
    int wordCount = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();
    if (wordCount < 10) {
        statusLabel_->setText("Text too short. Please provide at least 10 words for accurate detection.");
        return;
    }
    
    // Show loading state on button with animation
    statusLabel_->setText("Analyzing text...");
    analyzeButton_->setEnabled(false);
    clearButton_->setEnabled(false);
    textInput_->setEnabled(false);
    
    // Animate button with pulsing dots
    loadingDots_ = 0;
    loadingTimer_ = new QTimer(this);
    connect(loadingTimer_, &QTimer::timeout, [this]() {
        loadingDots_ = (loadingDots_ + 1) % 4;
        QString dots(loadingDots_, '.');
        QString spaces(3 - loadingDots_, ' ');
        analyzeButton_->setText("Analyzing" + dots + spaces);
    });
    loadingTimer_->start(400);
    
    // Run analysis in background thread
    QString textToAnalyze = text;
    auto detector = textDetector_;
    
    auto* watcher = new QFutureWatcher<std::pair<float, QString>>(this);
    connect(watcher, &QFutureWatcher<std::pair<float, QString>>::finished, [this, watcher]() {
        if (loadingTimer_) {
            loadingTimer_->stop();
            loadingTimer_->deleteLater();
            loadingTimer_ = nullptr;
        }
        
        try {
            auto result = watcher->result();
            onAnalysisComplete(result.first, result.second);
        } catch (const std::exception& e) {
            statusLabel_->setText("Error: " + QString::fromStdString(e.what()));
            Logger::error("AI text detection error: " + std::string(e.what()));
            
            // Re-enable UI
            analyzeButton_->setText("Analyze Text");
            analyzeButton_->setEnabled(true);
            clearButton_->setEnabled(true);
            textInput_->setEnabled(true);
        }
        
        watcher->deleteLater();
    });
    
    // Start async analysis
    QFuture<std::pair<float, QString>> future = QtConcurrent::run([detector, textToAnalyze]() -> std::pair<float, QString> {
        auto result = detector->analyze(textToAnalyze.toStdString());
        return std::make_pair(static_cast<float>(result.ai_score), QString::fromStdString(result.label));
    });
    
    watcher->setFuture(future);
}

void AITextDetectorPanel::onAnalysisComplete(float aiScore, const QString& label) {
    // Reset button text
    analyzeButton_->setText("Analyze Text");
    
    // Update UI with results
    updateResults(aiScore, label.toStdString());
    emit analysisComplete(aiScore);
    
    // Re-enable UI
    analyzeButton_->setEnabled(true);
    clearButton_->setEnabled(true);
    textInput_->setEnabled(true);
}

void AITextDetectorPanel::onClearText() {
    textInput_->clear();
    resultLabel_->setText("No analysis yet");
    scoreBar_->setValue(0);
    detailsLabel_->clear();
    statusLabel_->setText("Cleared");
}

void AITextDetectorPanel::updateResults(float aiScore, const std::string& details) {
    // Convert to percentage
    int scorePercent = static_cast<int>(aiScore * 100);
    scoreBar_->setValue(scorePercent);
    
    // Check for watermark only if AI score > 50%
    QString watermarkModel;
    if (aiScore > 0.5f) {
        watermarkModel = ChatbotPanel::extractWatermark(currentText_);
        Logger::info("Watermark check (AI score > 50%) - Model found: " + 
                    (watermarkModel.isEmpty() ? "none" : watermarkModel.toStdString()));
    }
    
    // Determine verdict
    QString verdict;
    QString color;
    
    if (aiScore >= 0.8f) {
        verdict = "Likely AI-Generated";
        color = "#cc0000"; // Red
    } else if (aiScore >= 0.5f) {
        verdict = "Possibly AI-Generated";
        color = "#ff9900"; // Orange
    } else if (aiScore >= 0.3f) {
        verdict = "Mixed/Uncertain";
        color = "#ffcc00"; // Yellow
    } else {
        verdict = "Likely Human-Written";
        color = "#00cc00"; // Green
    }
    
    // Append source model if watermark detected
    if (!watermarkModel.isEmpty()) {
        verdict += " (Source: " + watermarkModel + ")";
    }
    
    resultLabel_->setText(verdict);
    resultLabel_->setStyleSheet(
        QString("background-color: %1; "
                "color: white; "
                "border: 1px solid #ddd; "
                "border-radius: 5px; "
                "padding: 15px; "
                "font-size: 14pt; "
                "font-weight: bold;").arg(color)
    );
    
    // Details
    QString detailsText = QString(
        "<b>Confidence Score:</b> %1%<br>"
        "<b>Interpretation:</b><br>"
        "• 0-30%: Likely human-written<br>"
        "• 30-50%: Mixed or uncertain origin<br>"
        "• 50-80%: Possibly AI-generated<br>"
        "• 80-100%: Likely AI-generated<br><br>"
        "<i>Note: This is a statistical estimate and may not be 100% accurate. "
        "Factors like writing style, topic, and text length affect the results.</i>"
    ).arg(scorePercent);
    
    // Add source identification if watermark found
    if (!watermarkModel.isEmpty()) {
        detailsText = QString("<b>Source Identified:</b> <span style='color:#1565c0;'>%1</span><br><br>").arg(watermarkModel) + detailsText;
        statusLabel_->setText("Analysis complete - Source: " + watermarkModel);
    } else {
        statusLabel_->setText("Analysis complete");
    }
    
    if (!details.empty()) {
        detailsText += "<br><br><b>Additional Details:</b><br>" + 
                      QString::fromStdString(details).toHtmlEscaped();
    }
    
    detailsLabel_->setText(detailsText);
}

void AITextDetectorPanel::setTheme(bool isDark) {
    if (isDark) {
        // Dark theme
        textInput_->setStyleSheet(
            "QTextEdit { "
            "  border: 2px solid #444; "
            "  border-radius: 6px; "
            "  padding: 10px; "
            "  font-size: 18px; "
            "  background-color: #2d2d2d; "
            "  color: #e0e0e0; "
            "}"
            "QTextEdit:focus { "
            "  border-color: #4a90e2; "
            "}"
        );
        
        analyzeButton_->setStyleSheet(
            "QPushButton { "
            "  padding: 10px 24px; "
            "  background-color: #4a90e2; "
            "  color: white; "
            "  border: none; "
            "  border-radius: 8px; "
            "  font-size: 14px; "
            "  font-weight: 600; "
            "}"
            "QPushButton:hover { "
            "  background-color: #357abd; "
            "}"
            "QPushButton:pressed { "
            "  background-color: #2868a8; "
            "}"
            "QPushButton:disabled { "
            "  background-color: #555; "
            "  color: #999; "
            "}"
        );
        
    } else {
        // Light theme
        textInput_->setStyleSheet(
            "QTextEdit { "
            "  border: 2px solid #ced4da; "
            "  border-radius: 6px; "
            "  padding: 10px; "
            "  font-size: 18px; "
            "  background-color: white; "
            "  color: #333; "
            "}"
            "QTextEdit:focus { "
            "  border-color: #4a90e2; "
            "}"
        );
        
        analyzeButton_->setStyleSheet(
            "QPushButton { "
            "  padding: 10px 24px; "
            "  background-color: #4a90e2; "
            "  color: white; "
            "  border: none; "
            "  border-radius: 8px; "
            "  font-size: 14px; "
            "  font-weight: 600; "
            "}"
            "QPushButton:hover { "
            "  background-color: #357abd; "
            "}"
            "QPushButton:pressed { "
            "  background-color: #2868a8; "
            "}"
            "QPushButton:disabled { "
            "  background-color: #b0b0b0; "
            "  color: #e0e0e0; "
            "}"
        );
    }
}

} // namespace ModAI
