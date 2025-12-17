#include "ui/ChatbotPanel.h"
#include "utils/Logger.h"
#include "detectors/HiveTextModerator.h"
#include <nlohmann/json.hpp>
#include <QScrollBar>
#include <QMessageBox>
#include <QDateTime>
#include <QFrame>
#include <QTimer>

namespace ModAI {

ChatbotPanel::ChatbotPanel(QWidget* parent)
    : QWidget(parent)
    , chatScrollArea_(nullptr)
    , chatContainer_(nullptr)
    , chatLayout_(nullptr)
    , messageInput_(nullptr)
    , sendButton_(nullptr)
    , clearButton_(nullptr)
    , modelSelector_(nullptr)
    , statusLabel_(nullptr)
    , typingIndicator_(nullptr)
    , httpClient_(nullptr)
    , textModerator_(nullptr) {
    setupUI();
}

void ChatbotPanel::initialize(std::unique_ptr<HttpClient> httpClient,
                              std::unique_ptr<HiveTextModerator> textModerator,
                              const std::string& llmApiKey,
                              const std::string& llmEndpoint) {
    httpClient_ = std::move(httpClient);
    textModerator_ = std::move(textModerator);
    llmApiKey_ = llmApiKey.empty() ? "" : llmApiKey;
    
    // Always use the free GPT API endpoint
    llmEndpoint_ = "https://freeapi-v27x.onrender.com/v1/chat/completions";
    
    statusLabel_->setText("GPT-4o API configured with Hive railguard");
}

void ChatbotPanel::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 10, 0, 0);
    layout->setSpacing(10);
    
    // Header
    auto* headerLabel = new QLabel("🤖 AI Chatbot with Railguard");
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(14);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    headerLabel->setContentsMargins(10, 0, 10, 0);
    layout->addWidget(headerLabel);
    
    auto* infoLabel = new QLabel("Chat with an AI assistant. All responses are moderated by Hive API for safety.");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #666; margin-bottom: 5px;");
    infoLabel->setContentsMargins(10, 0, 10, 0);
    layout->addWidget(infoLabel);
    
    // Chat scroll area with message bubbles
    chatScrollArea_ = new QScrollArea;
    chatScrollArea_->setWidgetResizable(true);
    chatScrollArea_->setFrameShape(QFrame::NoFrame);
    chatScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    chatContainer_ = new QWidget;
    chatLayout_ = new QVBoxLayout(chatContainer_);
    chatLayout_->setSpacing(8);
    chatLayout_->setContentsMargins(10, 10, 10, 10);
    chatLayout_->addStretch();
    
    chatScrollArea_->setWidget(chatContainer_);
    chatScrollArea_->setStyleSheet(
        "QScrollArea { "
        "  background-color: #f5f5f5; "
        "  border: 1px solid #ddd; "
        "  border-radius: 8px; "
        "}"
    );
    layout->addWidget(chatScrollArea_, 1);
    
    // Input area
    auto* inputLayout = new QHBoxLayout;
    inputLayout->setContentsMargins(10, 0, 10, 10);
    inputLayout->setSpacing(10);
    
    messageInput_ = new QLineEdit;
    messageInput_->setPlaceholderText("Type your message here...");
    messageInput_->setMinimumHeight(45);
    messageInput_->setStyleSheet(
        "QLineEdit { "
        "  border: 2px solid #ddd; "
        "  border-radius: 22px; "
        "  padding: 10px 20px; "
        "  font-size: 14px; "
        "  background-color: white; "
        "}"
        "QLineEdit:focus { border-color: #0066cc; }"
    );
    
    sendButton_ = new QPushButton("Send");
    sendButton_->setMinimumWidth(90);
    sendButton_->setMinimumHeight(45);
    sendButton_->setStyleSheet(
        "QPushButton { "
        "  background-color: #0066cc; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 22px; "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "  padding: 0 25px; "
        "}"
        "QPushButton:hover { background-color: #0052a3; }"
        "QPushButton:pressed { background-color: #004080; }"
        "QPushButton:disabled { background-color: #ccc; }"
    );
    
    inputLayout->addWidget(messageInput_, 1);
    inputLayout->addWidget(sendButton_);
    layout->addLayout(inputLayout);
    
    // Model selection
    auto* modelLayout = new QHBoxLayout;
    auto* modelLabel = new QLabel("AI Model:");
    modelLabel->setStyleSheet("font-weight: bold; color: #333;");
    
    modelSelector_ = new QComboBox;
    modelSelector_->addItem("GPT-4o Mini (Fast, Vision)", "provider-5/gpt-4o-mini");
    modelSelector_->addItem("GPT-5 Nano (Reasoning, 64K)", "provider-5/gpt-5-nano");
    modelSelector_->addItem("DeepSeek R1 (Vision, Reasoning)", "provider-3/deepseek-r1-0528");
    modelSelector_->addItem("Llama 3.3 70B (Powerful)", "provider-3/llama-3.3-70b");
    modelSelector_->addItem("Qwen 2.5 72B (131K Context)", "provider-3/qwen-2.5-72b");
    modelSelector_->setCurrentIndex(0);
    modelSelector_->setMinimumWidth(300);
    modelSelector_->setStyleSheet(
        "QComboBox { "
        "  padding: 5px; "
        "  border: 1px solid #ccc; "
        "  border-radius: 5px; "
        "  background: white; "
        "}"
    );
    
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(modelSelector_);
    modelLayout->addStretch();
    layout->addLayout(modelLayout);
    
    // Controls
    auto* controlLayout = new QHBoxLayout;
    clearButton_ = new QPushButton("Clear Chat");
    statusLabel_ = new QLabel("Ready");
    statusLabel_->setStyleSheet("color: #666;");
    
    controlLayout->addWidget(clearButton_);
    controlLayout->addStretch();
    controlLayout->addWidget(statusLabel_);
    layout->addLayout(controlLayout);
    
    // Connections
    connect(sendButton_, &QPushButton::clicked, this, &ChatbotPanel::onSendMessage);
    connect(clearButton_, &QPushButton::clicked, this, &ChatbotPanel::onClearChat);
    connect(messageInput_, &QLineEdit::returnPressed, this, &ChatbotPanel::onSendMessage);
}

QWidget* ChatbotPanel::createMessageBubble(const QString& message, bool isUser, const QString& timestamp, bool blocked) {
    if (!chatContainer_) {
        return nullptr;
    }
    
    auto* bubbleContainer = new QWidget(chatContainer_);
    auto* containerLayout = new QHBoxLayout(bubbleContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);
    
    // Create the message bubble frame
    auto* bubble = new QFrame(bubbleContainer);
    bubble->setFrameShape(QFrame::StyledPanel);
    auto* bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(12, 8, 12, 8);
    bubbleLayout->setSpacing(4);
    
    // Message text
    auto* messageLabel = new QLabel(message, bubble);
    messageLabel->setWordWrap(true);
    messageLabel->setTextFormat(Qt::PlainText);
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont messageFont = messageLabel->font();
    messageFont.setPointSize(10);
    messageLabel->setFont(messageFont);
    
    // If blocked, initially hide the message
    if (blocked && !isUser) {
        messageLabel->hide();
        messageLabel->setProperty("actualMessage", message);
    }
    
    // Timestamp
    auto* timeLabel = new QLabel(timestamp, bubble);
    timeLabel->setAlignment(Qt::AlignRight);
    QFont timeFont = timeLabel->font();
    timeFont.setPointSize(8);
    timeLabel->setFont(timeFont);
    
    bubbleLayout->addWidget(messageLabel);
    
    // Add "Show Message" button for blocked content
    if (blocked && !isUser) {
        auto* warningLabel = new QLabel("Response blocked by safety filter", bubble);
        warningLabel->setWordWrap(true);
        QFont warningFont = warningLabel->font();
        warningFont.setPointSize(10);
        warningFont.setBold(true);
        warningLabel->setFont(warningFont);
        warningLabel->setStyleSheet("color: #c62828; margin-bottom: 4px;");
        bubbleLayout->insertWidget(0, warningLabel);
        
        auto* showButton = new QPushButton("Show Message Anyway", bubble);
        showButton->setStyleSheet(
            "QPushButton { "
            "  background-color: #c62828; "
            "  color: white; "
            "  border: none; "
            "  border-radius: 4px; "
            "  padding: 6px 12px; "
            "  font-size: 9pt; "
            "}"
            "QPushButton:hover { background-color: #b71c1c; }"
        );
        
        // Connect button to toggle message visibility
        connect(showButton, &QPushButton::clicked, [messageLabel, warningLabel, showButton]() {
            if (messageLabel->isVisible()) {
                // Hide message
                messageLabel->hide();
                warningLabel->setText("Response blocked by safety filter");
                showButton->setText("Show Message Anyway");
            } else {
                // Show message
                messageLabel->show();
                warningLabel->setText("This content was flagged as inappropriate:");
                showButton->setText("Hide Message");
            }
        });
        
        bubbleLayout->addWidget(showButton);
    }
    
    bubbleLayout->addWidget(timeLabel);
    
    // Style based on message type
    if (isUser) {
        // User bubble - right aligned, blue gradient
        containerLayout->addStretch();
        containerLayout->addWidget(bubble);
        bubble->setMaximumWidth(500);
        bubble->setStyleSheet(
            "QFrame { "
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "    stop:0 #0066cc, stop:1 #0052a3); "
            "  color: white; "
            "  border-radius: 18px; "
            "  border: none; "
            "}"
        );
        messageLabel->setStyleSheet("color: white;");
        timeLabel->setStyleSheet("color: rgba(255, 255, 255, 0.8);");
    } else {
        // Assistant bubble - left aligned, gray or red if blocked
        containerLayout->addWidget(bubble);
        containerLayout->addStretch();
        bubble->setMaximumWidth(500);
        
        if (blocked) {
            bubble->setStyleSheet(
                "QFrame { "
                "  background-color: #ffebee; "
                "  color: #c62828; "
                "  border-radius: 18px; "
                "  border: 1px solid #ef9a9a; "
                "}"
            );
            messageLabel->setStyleSheet("color: #c62828;");
            timeLabel->setStyleSheet("color: rgba(198, 40, 40, 0.7);");
        } else {
            bubble->setStyleSheet(
                "QFrame { "
                "  background-color: white; "
                "  color: #333; "
                "  border-radius: 18px; "
                "  border: 1px solid #e0e0e0; "
                "}"
            );
            messageLabel->setStyleSheet("color: #333;");
            timeLabel->setStyleSheet("color: rgba(0, 0, 0, 0.5);");
        }
    }
    
    return bubbleContainer;
}

void ChatbotPanel::scrollToBottom() {
    if (!chatScrollArea_ || !chatScrollArea_->verticalScrollBar()) {
        return;
    }
    
    // Force layout update
    chatContainer_->updateGeometry();
    chatScrollArea_->updateGeometry();
    
    // Schedule scroll to happen after UI updates
    QTimer::singleShot(100, this, [this]() {
        if (chatScrollArea_ && chatScrollArea_->verticalScrollBar()) {
            chatScrollArea_->verticalScrollBar()->setValue(
                chatScrollArea_->verticalScrollBar()->maximum()
            );
        }
    });
}

void ChatbotPanel::addMessageToChat(const QString& role, const QString& message, bool blocked) {
    if (!chatLayout_ || !chatContainer_) {
        // Can't log during initialization before Logger is ready
        return;
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm");
    bool isUser = (role == "User");
    
    // Remove stretch if present (always at the end before typing indicator)
    if (chatLayout_->count() > 0) {
        QLayoutItem* item = chatLayout_->itemAt(chatLayout_->count() - 1);
        if (item && item->spacerItem()) {
            chatLayout_->takeAt(chatLayout_->count() - 1);
            delete item;
        }
    }
    
    // Add the message bubble
    QWidget* bubble = createMessageBubble(message, isUser, timestamp, blocked);
    if (bubble) {
        chatLayout_->addWidget(bubble);
    }
    
    // Add stretch back at the end
    chatLayout_->addStretch();
    
    // Scroll to bottom
    scrollToBottom();
}

void ChatbotPanel::onSendMessage() {
    QString userMessage = messageInput_->text().trimmed();
    if (userMessage.isEmpty()) {
        return;
    }
    
    // Display user message
    addMessageToChat("User", userMessage);
    messageInput_->clear();
    
    // Store in conversation history
    conversationHistory_.push_back({"user", userMessage.toStdString()});
    
    // Call LLM
    statusLabel_->setText("🤔 Thinking...");
    sendButton_->setEnabled(false);
    showTypingIndicator();
    callLLM(userMessage);
}

void ChatbotPanel::onClearChat() {
    // Remove all message bubbles except the stretch
    while (chatLayout_->count() > 1) {
        QLayoutItem* item = chatLayout_->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    conversationHistory_.clear();
    statusLabel_->setText("Chat cleared");
}

void ChatbotPanel::callLLM(const QString& userMessage) {
    if (!httpClient_) {
        hideTypingIndicator();
        addMessageToChat("System", "Error: HTTP client not initialized", true);
        sendButton_->setEnabled(true);
        statusLabel_->setText("Error");
        return;
    }
    
    try {
        // Get selected model from dropdown
        QString selectedModel = modelSelector_->currentData().toString();
        
        // Construct request for custom GPT API endpoint
        nlohmann::json payload;
        payload["model"] = selectedModel.toStdString();
        
        // Add conversation history in OpenAI chat completions format
        nlohmann::json messages = nlohmann::json::array();
        messages.push_back({
            {"role", "system"},
            {"content", "You are a helpful, respectful AI assistant. Provide clear, accurate, and concise responses."}
        });
        
        for (const auto& [role, content] : conversationHistory_) {
            messages.push_back({{"role", role}, {"content", content}});
        }
        
        payload["messages"] = messages;
        
        HttpRequest req;
        req.url = llmEndpoint_;
        req.method = "POST";
        req.headers["Content-Type"] = "application/json";
        // No Authorization header needed for this free API
        req.body = payload.dump();
        
        Logger::info("Calling GPT API at: " + llmEndpoint_);
        
        // Make request (this should be async in production)
        HttpResponse response = httpClient_->post(req);
        
        hideTypingIndicator();
        
        if (!response.success) {
            addMessageToChat("System", "Error: " + QString::fromStdString(response.errorMessage), true);
            statusLabel_->setText("LLM Error");
            sendButton_->setEnabled(true);
            return;
        }
        
        if (response.statusCode != 200) {
            addMessageToChat("System", "API Error: Status " + QString::number(response.statusCode), true);
            statusLabel_->setText("API Error");
            sendButton_->setEnabled(true);
            return;
        }
        
        // Parse response in OpenAI format
        auto jsonResponse = nlohmann::json::parse(response.body);
        std::string llmResponse;
        
        // Parse OpenAI chat completions format
        if (jsonResponse.contains("choices") && !jsonResponse["choices"].empty()) {
            if (jsonResponse["choices"][0].contains("message") && 
                jsonResponse["choices"][0]["message"].contains("content")) {
                llmResponse = jsonResponse["choices"][0]["message"]["content"].get<std::string>();
            } else {
                addMessageToChat("System", "Error: Invalid response structure", true);
                statusLabel_->setText("Parse Error");
                sendButton_->setEnabled(true);
                return;
            }
        } else {
            addMessageToChat("System", "Error: Unexpected API response format", true);
            statusLabel_->setText("Parse Error");
            sendButton_->setEnabled(true);
            return;
        }
        
        // Add watermark based on selected model
        QString modelId = modelSelector_->currentData().toString();
        QString watermarkedResponse = addWatermark(QString::fromStdString(llmResponse), modelId);
        std::string watermarkedStdString = watermarkedResponse.toStdString();
        
        // Debug: Log watermark info
        Logger::info("Added watermark for model: " + modelId.toStdString() + ", text length: " + 
                    std::to_string(watermarkedResponse.length()) + " (original: " + 
                    std::to_string(llmResponse.length()) + ")");
        
        // Moderate the LLM response
        statusLabel_->setText("Moderating response...");
        bool passed = moderateResponse(llmResponse);
        
        if (passed) {
            addMessageToChat("Assistant", watermarkedResponse);
            conversationHistory_.push_back({"assistant", watermarkedStdString});
            statusLabel_->setText("Response sent");
            emit moderationResult(watermarkedResponse, true);
        } else {
            // Show blocked message with actual content (hidden by default)
            addMessageToChat("Assistant", watermarkedResponse, true);
            statusLabel_->setText("Response blocked");
            emit messageBlocked("Content failed moderation");
            emit moderationResult(watermarkedResponse, false);
        }
        
    } catch (const std::exception& e) {
        addMessageToChat("System", "Exception: " + QString::fromStdString(e.what()), true);
        statusLabel_->setText("Error");
        Logger::error("Chatbot error: " + std::string(e.what()));
    }
    
    sendButton_->setEnabled(true);
}

bool ChatbotPanel::moderateResponse(const std::string& response) {
    if (!textModerator_) {
        Logger::warn("No text moderator available - allowing response");
        return true;
    }
    
    try {
        // Use Hive API to moderate the text
        auto result = textModerator_->analyzeText(response);
        
        // Check if any category exceeds threshold
        // Lower threshold to catch more potentially harmful content
        const float threshold = 0.2f; // Block if score > 0.2 (20%)
        
        for (const auto& [category, score] : result.labels) {
            if (score > threshold) {
                Logger::info("LLM response blocked - " + category + ": " + std::to_string(score));
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        Logger::error("Moderation error: " + std::string(e.what()));
        // Fail closed - block on error
        return false;
    }
}

void ChatbotPanel::showTypingIndicator() {
    if (typingIndicator_ || !chatLayout_ || !chatContainer_) {
        return; // Already showing or not initialized
    }
    
    // Create typing indicator as a bubble
    auto* bubbleContainer = new QWidget(chatContainer_);
    auto* containerLayout = new QHBoxLayout(bubbleContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    
    typingIndicator_ = new QFrame(bubbleContainer);
    auto* bubbleLayout = new QHBoxLayout(typingIndicator_);
    bubbleLayout->setContentsMargins(16, 10, 16, 10);
    
    auto* typingLabel = new QLabel("💭 AI is thinking...", typingIndicator_);
    QFont font = typingLabel->font();
    font.setPointSize(9);
    font.setItalic(true);
    typingLabel->setFont(font);
    
    bubbleLayout->addWidget(typingLabel);
    
    typingIndicator_->setStyleSheet(
        "QFrame { "
        "  background-color: #e8e8e8; "
        "  color: #666; "
        "  border-radius: 18px; "
        "  border: 1px solid #d0d0d0; "
        "}"
    );
    typingLabel->setStyleSheet("color: #666;");
    
    typingIndicator_->setMaximumWidth(200);
    
    containerLayout->addWidget(typingIndicator_);
    containerLayout->addStretch();
    
    // Remove stretch temporarily
    if (chatLayout_->count() > 0) {
        QLayoutItem* item = chatLayout_->itemAt(chatLayout_->count() - 1);
        if (item && item->spacerItem()) {
            chatLayout_->takeAt(chatLayout_->count() - 1);
            delete item;
        }
    }
    
    // Add typing indicator
    chatLayout_->addWidget(bubbleContainer);
    chatLayout_->addStretch();
    
    // Scroll to bottom
    scrollToBottom();
}

void ChatbotPanel::hideTypingIndicator() {
    if (!typingIndicator_) {
        return;
    }
    
    // Find and remove the typing indicator's parent widget
    for (int i = chatLayout_->count() - 1; i >= 0; --i) {
        QLayoutItem* item = chatLayout_->itemAt(i);
        if (!item || item->spacerItem()) {
            continue;
        }
        
        QWidget* widget = item->widget();
        if (!widget) {
            continue;
        }
        
        // Check if this widget contains our typing indicator
        auto* frame = widget->findChild<QFrame*>();
        if (frame == typingIndicator_) {
            chatLayout_->removeWidget(widget);
            widget->deleteLater();
            typingIndicator_ = nullptr;
            break;
        }
    }
}

void ChatbotPanel::setTheme(bool isDark) {
    isDarkTheme_ = isDark;
    
    if (isDark) {
        // Dark theme
        chatScrollArea_->setStyleSheet(
            "QScrollArea { "
            "  background-color: #1e1e1e; "
            "  border: 1px solid #333; "
            "  border-radius: 8px; "
            "}"
        );
        
        chatContainer_->setStyleSheet(
            "QWidget { background-color: #1e1e1e; }"
        );
        
        messageInput_->setStyleSheet(
            "QLineEdit { "
            "  border: 2px solid #444; "
            "  border-radius: 22px; "
            "  padding: 10px 20px; "
            "  font-size: 14px; "
            "  background-color: #2d2d2d; "
            "  color: #e0e0e0; "
            "}"
            "QLineEdit:focus { border-color: #0066cc; }"
        );
        
        modelSelector_->setStyleSheet(
            "QComboBox { "
            "  padding: 5px; "
            "  border: 1px solid #444; "
            "  border-radius: 5px; "
            "  background: #2d2d2d; "
            "  color: #e0e0e0; "
            "}"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { "
            "  background-color: #2d2d2d; "
            "  color: #e0e0e0; "
            "  selection-background-color: #0066cc; "
            "}"
        );
        
        statusLabel_->setStyleSheet("color: #aaa;");
        
    } else {
        // Light theme
        chatScrollArea_->setStyleSheet(
            "QScrollArea { "
            "  background-color: #f5f5f5; "
            "  border: 1px solid #ddd; "
            "  border-radius: 8px; "
            "}"
        );
        
        chatContainer_->setStyleSheet(
            "QWidget { background-color: #f5f5f5; }"
        );
        
        messageInput_->setStyleSheet(
            "QLineEdit { "
            "  border: 2px solid #ddd; "
            "  border-radius: 22px; "
            "  padding: 10px 20px; "
            "  font-size: 14px; "
            "  background-color: white; "
            "  color: #333; "
            "}"
            "QLineEdit:focus { border-color: #0066cc; }"
        );
        
        modelSelector_->setStyleSheet(
            "QComboBox { "
            "  padding: 5px; "
            "  border: 1px solid #ccc; "
            "  border-radius: 5px; "
            "  background: white; "
            "  color: #333; "
            "}"
            "QComboBox QAbstractItemView { "
            "  background-color: white; "
            "  color: #333; "
            "  selection-background-color: #0066cc; "
            "}"
        );
        
        statusLabel_->setStyleSheet("color: #666;");
    }
}

/**
 * @brief Add invisible watermark to text using zero-width Unicode characters
 * 
 * Embeds model information as binary pattern using:
 * - U+200B (Zero Width Space) = 0
 * - U+200C (Zero Width Non-Joiner) = 1
 * - U+200D (Zero Width Joiner) = separator
 * - U+FEFF (Zero Width No-Break Space) = start marker
 */
QString ChatbotPanel::addWatermark(const QString& text, const QString& modelId) {
    // Model ID to binary encoding
    QMap<QString, QString> modelEncoding;
    modelEncoding["provider-5/gpt-4o-mini"] = "00001";
    modelEncoding["provider-5/gpt-5-nano"] = "00010";
    modelEncoding["provider-3/deepseek-r1-0528"] = "00011";
    modelEncoding["provider-3/llama-3.3-70b"] = "00100";
    modelEncoding["provider-3/qwen-2.5-72b"] = "00101";
    
    QString binary = modelEncoding.value(modelId, "00000");
    
    // Build watermark using zero-width characters
    QString watermark;
    watermark += QChar(0xFEFF); // Start marker (Zero Width No-Break Space)
    
    for (QChar bit : binary) {
        if (bit == '0') {
            watermark += QChar(0x200B); // Zero Width Space
        } else {
            watermark += QChar(0x200C); // Zero Width Non-Joiner
        }
    }
    
    watermark += QChar(0x200D); // End marker (Zero Width Joiner)
    
    // Insert watermark at the beginning of text
    return watermark + text;
}

/**
 * @brief Extract watermark from text if present
 * @return Model identifier or empty string if no watermark found
 */
QString ChatbotPanel::extractWatermark(const QString& text) {
    // Debug: Log first 20 characters with their Unicode codepoints
    QString debugStr;
    for (int i = 0; i < qMin(20, text.length()); ++i) {
        debugStr += QString("U+%1 ").arg(text[i].unicode(), 4, 16, QChar('0'));
    }
    Logger::info("Watermark extraction - First 20 chars: " + debugStr.toStdString());
    
    // Look for start marker
    int startPos = text.indexOf(QChar(0xFEFF));
    if (startPos == -1) {
        Logger::info("Watermark extraction - No start marker (U+FEFF) found");
        return "";
    }
    
    Logger::info("Watermark extraction - Start marker found at position " + std::to_string(startPos));
    
    // Extract binary pattern
    QString binary;
    int pos = startPos + 1;
    
    while (pos < text.length() && pos < startPos + 10) {
        QChar ch = text[pos];
        if (ch == QChar(0x200B)) {
            binary += "0";
        } else if (ch == QChar(0x200C)) {
            binary += "1";
        } else if (ch == QChar(0x200D)) {
            break; // End marker found
        } else {
            Logger::info("Watermark extraction - Invalid character U+" + 
                        QString::number(ch.unicode(), 16).toStdString() + " at position " + std::to_string(pos));
            return ""; // Invalid watermark
        }
        pos++;
    }
    
    Logger::info("Watermark extraction - Binary pattern: " + binary.toStdString());
    
    // Decode binary to model name
    QMap<QString, QString> binaryToModel;
    binaryToModel["00001"] = "GPT-4o-mini";
    binaryToModel["00010"] = "GPT-5-nano";
    binaryToModel["00011"] = "DeepSeek-R1";
    binaryToModel["00100"] = "Llama-3.3-70B";
    binaryToModel["00101"] = "Qwen-2.5-72B";
    
    QString result = binaryToModel.value(binary, "");
    Logger::info("Watermark extraction - Decoded model: " + (result.isEmpty() ? "none (invalid binary)" : result.toStdString()));
    return result;
}

} // namespace ModAI
