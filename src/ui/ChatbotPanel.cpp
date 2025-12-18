#include "ui/ChatbotPanel.h"
#include "utils/Logger.h"
#include "detectors/HiveTextModerator.h"
#include <nlohmann/json.hpp>
#include <QScrollBar>
#include <QMessageBox>
#include <QDateTime>
#include <QFrame>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QImageReader>
#include <QImageWriter>
#include <QBuffer>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <cstdlib>

namespace ModAI {

// Constants for image metadata embedding
static const QString AI_GENERATED_MARKER = "ModAI-Generated";
static const QString METADATA_KEY = "ModAI-Source";

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

void ChatbotPanel::setImageModerator(std::shared_ptr<ImageModerator> imageModerator) {
    imageModerator_ = imageModerator;
    if (imageModerator_) {
        Logger::info("Image moderator configured for chatbot");
    }
}

void ChatbotPanel::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    
    // Header with title and description
    auto* headerWidget = new QWidget;
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(8, 8, 8, 16);
    headerLayout->setSpacing(12);
    
    auto* headerLabel = new QLabel("AI Chatbot with Railguard");
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(20);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    headerLabel->setStyleSheet("color: #2c3e50;");
    headerLayout->addWidget(headerLabel);
    
    auto* descLabel = new QLabel("Chat with AI models with automated content moderation using Hive API");
    QFont descFont = descLabel->font();
    descFont.setPointSize(12);
    descLabel->setFont(descFont);
    descLabel->setStyleSheet("color: #6c757d;");
    descLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    headerLayout->addWidget(descLabel);
    
    headerLayout->addStretch();
    layout->addWidget(headerWidget);
    
    // Chat Area
    auto* chatFrame = new QFrame;
    chatFrame->setStyleSheet(
        "QFrame { background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 8px; }"
    );
    auto* chatFrameLayout = new QVBoxLayout(chatFrame);
    chatFrameLayout->setContentsMargins(0, 0, 0, 0);
    chatFrameLayout->setSpacing(0);
    
    // Chat scroll area with message bubbles
    chatScrollArea_ = new QScrollArea;
    chatScrollArea_->setWidgetResizable(true);
    chatScrollArea_->setFrameShape(QFrame::NoFrame);
    chatScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    chatContainer_ = new QWidget;
    chatLayout_ = new QVBoxLayout(chatContainer_);
    chatLayout_->setSpacing(8);
    chatLayout_->setContentsMargins(20, 20, 20, 20);
    chatLayout_->addStretch();
    
    chatScrollArea_->setWidget(chatContainer_);
    chatScrollArea_->setStyleSheet(
        "QScrollArea { "
        "  background-color: #f8f9fa; "
        "  border: none; "
        "  border-radius: 8px; "
        "}"
    );
    chatFrameLayout->addWidget(chatScrollArea_);
    layout->addWidget(chatFrame, 1);
    
    // Input Area
    auto* inputFrame = new QFrame;
    inputFrame->setStyleSheet(
        "QFrame { background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 8px; }"
    );
    auto* inputLayout = new QHBoxLayout(inputFrame);
    inputLayout->setContentsMargins(12, 12, 12, 12);
    inputLayout->setSpacing(12);
    
    messageInput_ = new QLineEdit;
    messageInput_->setPlaceholderText("Type your message here...");
    messageInput_->setMinimumHeight(55);
    messageInput_->setStyleSheet(
        "QLineEdit { "
        "  border: 2px solid #e0e0e0; "
        "  border-radius: 8px; "
        "  padding: 12px 18px; "
        "  font-size: 13pt; "
        "  background-color: #fafafa; "
        "}"
        "QLineEdit:focus { "
        "  border-color: #4a90e2; "
        "  background-color: white; "
        "}"
    );
    
    sendButton_ = new QPushButton("Send");
    sendButton_->setMinimumWidth(100);
    sendButton_->setMinimumHeight(45);
    sendButton_->setStyleSheet(
        "QPushButton { "
        "  background-color: #4a90e2; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 8px; "
        "  font-weight: 600; "
        "  font-size: 16px; "
        "  padding: 10px 24px; "
        "}"
        "QPushButton:hover { background-color: #357abd; }"
        "QPushButton:pressed { background-color: #2a6496; }"
        "QPushButton:disabled { background-color: #d0d0d0; }"
    );
    
    inputLayout->addWidget(messageInput_, 1);
    inputLayout->addWidget(sendButton_);
    layout->addWidget(inputFrame);
    
    // Model Selection and Controls
    auto* controlsWidget = new QWidget;
    auto* controlsLayout = new QHBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(12);
    
    auto* modelLabel = new QLabel("AI Model:");
    modelLabel->setStyleSheet("font-weight: bold; color: #2c3e50; font-size: 16pt; border: none; background: transparent;");
    
    modelSelector_ = new QComboBox;
    modelSelector_->addItem("GPT-4o Mini (Fast, Vision)", "provider-5/gpt-4o-mini");
    modelSelector_->addItem("GPT-5 Nano (Reasoning, 64K)", "provider-5/gpt-5-nano");
    modelSelector_->addItem("DeepSeek R1 (Vision, Reasoning)", "provider-3/deepseek-r1-0528");
    modelSelector_->addItem("Llama 3.3 70B (Powerful)", "provider-3/llama-3.3-70b");
    modelSelector_->addItem("Qwen 2.5 72B (131K Context)", "provider-3/qwen-2.5-72b");
    modelSelector_->addItem("--- Image Generation ---", "separator");
    modelSelector_->addItem("FLUX Schnell (Fast Image)", "black-forest-labs/flux-schnell");
    modelSelector_->addItem("FLUX Dev (High Quality)", "black-forest-labs/flux-dev");
    modelSelector_->setCurrentIndex(0);
    modelSelector_->setMinimumWidth(300);
    modelSelector_->setStyleSheet(
        "QComboBox { "
        "  padding: 10px 32px 10px 14px; "
        "  border: 2px solid #e0e0e0; "
        "  border-radius: 8px; "
        "  background-color: #fafafa; "
        "  font-size: 16px; "
        "}"
        "QComboBox:focus { "
        "  border-color: #4a90e2; "
        "  background-color: white; "
        "}"
        "QComboBox::drop-down { "
        "  subcontrol-origin: padding; "
        "  subcontrol-position: top right; "
        "  width: 30px; "
        "  border-left: 1px solid #e0e0e0; "
        "  border-top-right-radius: 8px; "
        "  border-bottom-right-radius: 8px; "
        "}"
        "QComboBox::down-arrow { "
        "  image: none; "
        "  border-left: 5px solid transparent; "
        "  border-right: 5px solid transparent; "
        "  border-top: 6px solid #666; "
        "  width: 0; "
        "  height: 0; "
        "  margin-right: 8px; "
        "}"
        "QComboBox:hover::drop-down { "
        "  background-color: #f0f0f0; "
        "}"
        "QComboBox QAbstractItemView { "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 8px; "
        "  background-color: white; "
        "  selection-background-color: #4a90e2; "
        "  selection-color: white; "
        "  padding: 4px; "
        "}"
    );
    
    clearButton_ = new QPushButton("Clear Chat");
    clearButton_->setMinimumHeight(40);
    clearButton_->setStyleSheet(
        "QPushButton { "
        "  background-color: #6c757d; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 8px; "
        "  font-weight: 600; "
        "  font-size: 16px; "
        "  padding: 10px 24px; "
        "}"
        "QPushButton:hover { background-color: #5a6268; }"
        "QPushButton:pressed { background-color: #495057; }"
    );
    
    statusLabel_ = new QLabel("Ready");
    statusLabel_->setStyleSheet("color: #7f8c8d; font-size: 14px;");
    
    controlsLayout->addWidget(modelLabel);
    controlsLayout->addWidget(modelSelector_);
    controlsLayout->addWidget(clearButton_);
    controlsLayout->addStretch();
    controlsLayout->addWidget(statusLabel_);
    layout->addWidget(controlsWidget);
    
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
    messageFont.setPointSize(13);
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
        bubble->setMaximumWidth(700);
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
        bubble->setMaximumWidth(700);
        
        if (blocked) {
            bubble->setStyleSheet(
                "QFrame { "
                "  background-color: #ffebee; "
                "  color: #c62828; "
                "  border-radius: 18px; "
                "  border: none; "
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
                "  border: none; "
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
    
    // Check if using image generation model
    QString selectedModel = modelSelector_->currentData().toString();
    bool isImageModel = selectedModel.startsWith("black-forest-labs/");
    
    if (isImageModel) {
        // Image generation mode
        statusLabel_->setText("Generating image...");
        sendButton_->setEnabled(false);
        showTypingIndicator();
        generateImage(userMessage);
    } else {
        // Store in conversation history (only for text models)
        conversationHistory_.push_back({"user", userMessage.toStdString()});
        
        // Call LLM
        statusLabel_->setText("Thinking...");
        sendButton_->setEnabled(false);
        showTypingIndicator();
        callLLM(userMessage);
    }
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

void ChatbotPanel::generateImage(const QString& prompt) {
    if (!httpClient_) {
        hideTypingIndicator();
        addMessageToChat("System", "Error: HTTP client not initialized", true);
        sendButton_->setEnabled(true);
        statusLabel_->setText("Error");
        return;
    }
    
    try {
        QString selectedModel = modelSelector_->currentData().toString();
        QString modelDisplayName = modelSelector_->currentText();
        
        // Construct request for Nebius image API
        nlohmann::json payload;
        payload["model"] = selectedModel.toStdString();
        payload["prompt"] = prompt.toStdString();
        
        HttpRequest req;
        req.url = "https://api.tokenfactory.nebius.com/v1/images/generations";
        req.method = "POST";
        req.headers["Content-Type"] = "application/json";
        req.headers["Accept"] = "*/*";
        
        // Get API key from environment
        const char* apiKey = std::getenv("NEBIUS_API_KEY");
        if (!apiKey || std::string(apiKey).empty()) {
            hideTypingIndicator();
            addMessageToChat("System", "Error: NEBIUS_API_KEY environment variable not set", true);
            sendButton_->setEnabled(true);
            statusLabel_->setText("API key missing");
            return;
        }
        req.headers["Authorization"] = "Bearer " + std::string(apiKey);
        req.body = payload.dump();
        
        Logger::info("Calling Nebius Image API with model: " + selectedModel.toStdString());
        
        HttpResponse response = httpClient_->post(req);
        
        if (response.statusCode != 200) {
            hideTypingIndicator();
            addMessageToChat("System", "API Error: Status " + QString::number(response.statusCode), true);
            statusLabel_->setText("API Error");
            sendButton_->setEnabled(true);
            return;
        }
        
        // Parse response
        auto jsonResponse = nlohmann::json::parse(response.body);
        
        if (jsonResponse.contains("data") && !jsonResponse["data"].empty()) {
            std::string imageUrl = jsonResponse["data"][0]["url"].get<std::string>();
            Logger::info("Image generated: " + imageUrl);
            
            // Download and moderate the image
            statusLabel_->setText("Moderating image...");
            addImageToChat(QString::fromStdString(imageUrl), prompt, false);
        } else {
            hideTypingIndicator();
            addMessageToChat("System", "Error: No image in response", true);
            statusLabel_->setText("Generation failed");
        }
        
    } catch (const std::exception& e) {
        hideTypingIndicator();
        addMessageToChat("System", "Exception: " + QString::fromStdString(e.what()), true);
        statusLabel_->setText("Error");
        Logger::error("Image generation error: " + std::string(e.what()));
    }
    
    sendButton_->setEnabled(true);
}

bool ChatbotPanel::moderateImage(const QByteArray& imageData, const QString& mimeType) {
    if (!imageModerator_) {
        Logger::warn("Image moderator not configured - skipping moderation");
        return true; // Allow if no moderator
    }
    
    try {
        std::vector<uint8_t> imageBytes(imageData.begin(), imageData.end());
        auto result = imageModerator_->analyzeImage(imageBytes, mimeType.toStdString());
        
        // Define harmful categories to check - all "yes_*" and positive indicators
        static const std::vector<std::string> harmfulCategories = {
            // Sexual content
            "general_nsfw",
            "general_suggestive",
            "yes_female_nudity",
            "yes_male_nudity",
            "yes_sexual_activity",
            "yes_sexual_intent",
            "yes_female_underwear",
            "yes_male_underwear",
            "yes_sex_toy",
            "yes_realistic_nsfw",
            "yes_undressed",
            "yes_cleavage",
            "yes_bra",
            "yes_panties",
            "yes_negligee",
            "yes_female_swimwear",
            "yes_male_shirtless",
            "yes_miniskirt",
            "yes_bodysuit",
            "yes_sports_bra",
            "yes_sportswear_bottoms",
            "yes_bulge",
            "yes_breast",
            "yes_genitals",
            "yes_butt",
            // Violence
            "gun_in_hand",
            "gun_not_in_hand",
            "animated_gun",
            "knife_in_hand",
            "knife_not_in_hand",
            "culinary_knife_in_hand",
            "culinary_knife_not_in_hand",
            "very_bloody",
            "a_little_bloody",
            "other_blood",
            "yes_fight",
            "human_corpse",
            "animated_corpse",
            "yes_self_harm",
            "hanging",
            "noose",
            // Drugs
            "yes_pills",
            "yes_smoking",
            "illicit_injectables",
            "medical_injectables",
            "yes_marijuana",
            "yes_alcohol",
            "yes_drinking_alcohol",
            "animated_alcohol",
            // Hate symbols
            "yes_nazi",
            "yes_kkk",
            "yes_terrorist",
            "yes_confederate",
            // Other harmful
            "yes_middle_finger",
            "yes_child_present",
            "yes_animal_abuse",
            "yes_emaciated_body",
            "yes_gambling",
            "yes_religious_icon",
            "kissing",
            "licking",
            "animal_genitalia_and_human",
            "animal_genitalia_only",
            "animated_animal_genitalia"
        };
        
        // Log all labels for debugging (exclude no_ labels which indicate safe content)
        Logger::debug("Hive Visual Moderation labels (score > 0.1):");
        for (const auto& [category, score] : result.labels) {
            if (score > 0.1 && category.substr(0, 3) != "no_") {
                Logger::debug("  " + category + ": " + std::to_string(score));
            }
        }
        
        // Check for harmful content - single threshold for all
        const float threshold = 0.5f;  // 50% threshold
        
        for (const auto& category : harmfulCategories) {
            auto it = result.labels.find(category);
            if (it != result.labels.end()) {
                float score = static_cast<float>(it->second);
                
                if (score >= threshold) {
                    Logger::info("Image blocked - " + category + ": " + std::to_string(score));
                    return false;
                }
            }
        }
        
        Logger::info("Image passed moderation");
        return true;
    } catch (const std::exception& e) {
        Logger::error("Image moderation error: " + std::string(e.what()));
        return false; // Fail closed
    }
}

QByteArray ChatbotPanel::embedImageMetadata(const QByteArray& imageData, const QString& source) {
    // Load the image
    QImage image;
    image.loadFromData(imageData);
    
    if (image.isNull()) {
        Logger::warn("Could not load image for metadata embedding");
        return imageData;
    }
    
    // Add text metadata to the image
    image.setText(METADATA_KEY, source);
    image.setText("AI-Generated", AI_GENERATED_MARKER);
    image.setText("Generator", "ModAI-Chatbot");
    image.setText("Timestamp", QDateTime::currentDateTime().toString(Qt::ISODate));
    
    // Save as PNG to preserve metadata
    QByteArray output;
    QBuffer buffer(&output);
    buffer.open(QIODevice::WriteOnly);
    
    QImageWriter writer(&buffer, "PNG");
    writer.setText(METADATA_KEY, source);
    writer.setText("AI-Generated", AI_GENERATED_MARKER);
    
    if (writer.write(image)) {
        Logger::info("Embedded metadata in image: " + source.toStdString());
        return output;
    }
    
    // Fallback: save without custom writer
    image.save(&buffer, "PNG");
    return output;
}

QString ChatbotPanel::extractImageMetadata(const QString& imagePath) {
    QImageReader reader(imagePath);
    
    // Try to read metadata
    QString source = reader.text(METADATA_KEY);
    if (!source.isEmpty()) {
        return source;
    }
    
    // Alternative: check for AI-Generated marker
    QString aiMarker = reader.text("AI-Generated");
    if (aiMarker == AI_GENERATED_MARKER) {
        QString generator = reader.text("Generator");
        if (!generator.isEmpty()) {
            return generator;
        }
        return "Unknown AI Generator";
    }
    
    return QString(); // No metadata found
}

void ChatbotPanel::addBlockedImageToChat(const QString& prompt, const QString& reason) {
    // Create blocked image bubble - styled like blocked message
    auto* bubbleContainer = new QWidget(chatContainer_);
    auto* containerLayout = new QHBoxLayout(bubbleContainer);
    containerLayout->setContentsMargins(10, 5, 10, 5);
    
    auto* bubble = new QFrame(bubbleContainer);
    bubble->setMaximumWidth(420);
    
    auto* bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(12, 10, 12, 10);
    bubbleLayout->setSpacing(6);
    
    // Blocked icon and message
    auto* blockedLabel = new QLabel("[Image Blocked by Hive Moderation]", bubble);
    blockedLabel->setAlignment(Qt::AlignCenter);
    blockedLabel->setStyleSheet(
        "QLabel { "
        "  color: #c62828; "
        "  font-weight: bold; "
        "  font-size: 11pt; "
        "  padding: 20px; "
        "}"
    );
    bubbleLayout->addWidget(blockedLabel);
    
    // Reason
    auto* reasonLabel = new QLabel("Reason: " + reason, bubble);
    reasonLabel->setWordWrap(true);
    reasonLabel->setAlignment(Qt::AlignCenter);
    reasonLabel->setStyleSheet("color: #c62828; font-size: 9pt;");
    bubbleLayout->addWidget(reasonLabel);
    
    // Original prompt
    auto* promptLabel = new QLabel("Prompt: " + prompt, bubble);
    promptLabel->setWordWrap(true);
    promptLabel->setStyleSheet("color: #666; font-size: 9pt; font-style: italic; padding: 4px 0px;");
    bubbleLayout->addWidget(promptLabel);
    
    // Timestamp
    auto* timeLabel = new QLabel(QDateTime::currentDateTime().toString("hh:mm"), bubble);
    timeLabel->setAlignment(Qt::AlignRight);
    timeLabel->setStyleSheet("color: rgba(198, 40, 40, 0.7); font-size: 8pt;");
    bubbleLayout->addWidget(timeLabel);
    
    // Blocked style (red)
    bubble->setStyleSheet(
        "QFrame { "
        "  background-color: #ffebee; "
        "  color: #c62828; "
        "  border-radius: 18px; "
        "  border: none; "
        "}"
    );
    
    containerLayout->addWidget(bubble);
    containerLayout->addStretch();
    
    // Add to layout
    if (chatLayout_->count() > 0) {
        QLayoutItem* item = chatLayout_->itemAt(chatLayout_->count() - 1);
        if (item && item->spacerItem()) {
            chatLayout_->takeAt(chatLayout_->count() - 1);
            delete item;
        }
    }
    
    chatLayout_->addWidget(bubbleContainer);
    chatLayout_->addStretch();
    scrollToBottom();
    
    emit imageBlocked(reason);
}

void ChatbotPanel::addImageToChat(const QString& imageUrl, const QString& prompt, bool blocked) {
    // Create image bubble - styled like assistant chat bubble (left-aligned, white)
    auto* bubbleContainer = new QWidget(chatContainer_);
    auto* containerLayout = new QHBoxLayout(bubbleContainer);
    containerLayout->setContentsMargins(10, 5, 10, 5);
    
    auto* bubble = new QFrame(bubbleContainer);
    bubble->setMaximumWidth(420);
    
    auto* bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(12, 10, 12, 10);
    bubbleLayout->setSpacing(6);
    
    // Image label (placeholder while loading)
    auto* imageLabel = new QLabel(bubble);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setMinimumSize(300, 200);
    imageLabel->setText("Loading and moderating...");
    imageLabel->setStyleSheet(
        "QLabel { "
        "  background-color: #f5f5f5; "
        "  border-radius: 12px; "
        "  padding: 10px; "
        "  color: #666; "
        "}"
    );
    bubbleLayout->addWidget(imageLabel);
    
    // Prompt caption below image
    auto* promptLabel = new QLabel(prompt, bubble);
    promptLabel->setWordWrap(true);
    promptLabel->setStyleSheet("color: #555; font-size: 9pt; font-style: italic; padding: 4px 0px;");
    bubbleLayout->addWidget(promptLabel);
    
    // Source label (hidden by default, shown after loading)
    auto* sourceLabel = new QLabel(bubble);
    sourceLabel->setWordWrap(true);
    sourceLabel->setStyleSheet("color: #0066cc; font-size: 8pt; padding: 2px 0px;");
    sourceLabel->hide();
    bubbleLayout->addWidget(sourceLabel);
    
    // Download button (hidden by default, shown after loading)
    auto* downloadBtn = new QPushButton("Save Image", bubble);
    downloadBtn->setStyleSheet(
        "QPushButton { "
        "  background-color: #4CAF50; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 12px; "
        "  padding: 8px 16px; "
        "  font-size: 9pt; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #45a049; } "
        "QPushButton:pressed { background-color: #3d8b40; }"
    );
    downloadBtn->setCursor(Qt::PointingHandCursor);
    downloadBtn->hide();
    bubbleLayout->addWidget(downloadBtn);
    
    // Timestamp
    auto* timeLabel = new QLabel(QDateTime::currentDateTime().toString("hh:mm"), bubble);
    timeLabel->setAlignment(Qt::AlignRight);
    timeLabel->setStyleSheet("color: rgba(0, 0, 0, 0.5); font-size: 8pt;");
    bubbleLayout->addWidget(timeLabel);
    
    // Style like assistant bubble
    bubble->setStyleSheet(
        "QFrame { "
        "  background-color: white; "
        "  color: #333; "
        "  border-radius: 18px; "
        "  border: none; "
        "}"
    );
    
    // Left-aligned like assistant messages
    containerLayout->addWidget(bubble);
    containerLayout->addStretch();
    
    // Remove the existing stretch at the end of the chat layout
    if (chatLayout_->count() > 0) {
        QLayoutItem* item = chatLayout_->itemAt(chatLayout_->count() - 1);
        if (item && item->spacerItem()) {
            chatLayout_->takeAt(chatLayout_->count() - 1);
            delete item;
        }
    }
    
    chatLayout_->addWidget(bubbleContainer);
    chatLayout_->addStretch();
    
    // Get model info for metadata
    QString selectedModel = modelSelector_->currentData().toString();
    QString modelName = modelSelector_->currentText();
    
    // Download, moderate, and display image asynchronously
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QNetworkRequest request{QUrl(imageUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    connect(manager, &QNetworkAccessManager::finished, 
            [this, imageLabel, sourceLabel, downloadBtn, imageUrl, prompt, bubbleContainer, bubble, manager, selectedModel, modelName](QNetworkReply* reply) {
        hideTypingIndicator();
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray imageData = reply->readAll();
            
            // Determine MIME type
            QString mimeType = "image/webp";
            if (imageUrl.toLower().endsWith(".png")) {
                mimeType = "image/png";
            } else if (imageUrl.toLower().endsWith(".jpg") || imageUrl.toLower().endsWith(".jpeg")) {
                mimeType = "image/jpeg";
            }
            
            // Moderate the image
            bool passedModeration = moderateImage(imageData, mimeType);
            
            if (!passedModeration) {
                // Image blocked - update bubble to show blocked state with "Show Anyway" option
                imageLabel->setText("[Image blocked by Hive moderation]\n\nThe generated image was flagged for potentially inappropriate content.");
                imageLabel->setStyleSheet(
                    "QLabel { "
                    "  background-color: #ffebee; "
                    "  border-radius: 12px; "
                    "  padding: 20px; "
                    "  color: #c62828; "
                    "  font-weight: bold; "
                    "}"
                );
                bubble->setStyleSheet(
                    "QFrame { "
                    "  background-color: #ffebee; "
                    "  color: #c62828; "
                    "  border-radius: 18px; "
                    "  border: 1px solid #ef9a9a; "
                    "}"
                );
                
                // Add "Show Anyway" button
                auto* showAnywayBtn = new QPushButton("Show Anyway", bubble);
                showAnywayBtn->setStyleSheet(
                    "QPushButton { "
                    "  background-color: #ef5350; "
                    "  color: white; "
                    "  border: none; "
                    "  border-radius: 12px; "
                    "  padding: 8px 16px; "
                    "  font-size: 10pt; "
                    "}"
                    "QPushButton:hover { background-color: #e53935; }"
                );
                qobject_cast<QVBoxLayout*>(bubble->layout())->insertWidget(1, showAnywayBtn, 0, Qt::AlignCenter);
                
                // Add "Hide Image" button (hidden initially)
                auto* hideImageBtn = new QPushButton("Hide Image", bubble);
                hideImageBtn->setStyleSheet(
                    "QPushButton { "
                    "  background-color: #78909c; "
                    "  color: white; "
                    "  border: none; "
                    "  border-radius: 12px; "
                    "  padding: 8px 16px; "
                    "  font-size: 10pt; "
                    "}"
                    "QPushButton:hover { background-color: #546e7a; }"
                );
                qobject_cast<QVBoxLayout*>(bubble->layout())->insertWidget(2, hideImageBtn, 0, Qt::AlignCenter);
                hideImageBtn->hide();
                
                // Connect show anyway button
                connect(showAnywayBtn, &QPushButton::clicked, [this, imageLabel, sourceLabel, downloadBtn, showAnywayBtn, hideImageBtn, bubble, imageData, selectedModel, modelName, prompt]() {
                    // Embed metadata and display (bypassing moderation)
                    QByteArray imageWithMetadata = embedImageMetadata(imageData, selectedModel);
                    
                    QImage image;
                    image.loadFromData(imageWithMetadata.isEmpty() ? imageData : imageWithMetadata);
                    
                    if (image.isNull()) {
                        QBuffer buffer;
                        buffer.setData(imageData);
                        buffer.open(QIODevice::ReadOnly);
                        QImageReader reader(&buffer);
                        reader.setAutoDetectImageFormat(true);
                        image = reader.read();
                    }
                    
                    if (!image.isNull()) {
                        QPixmap pixmap = QPixmap::fromImage(image);
                        QPixmap scaled = pixmap.scaled(380, 380, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        imageLabel->setPixmap(scaled);
                        imageLabel->setStyleSheet("QLabel { background: transparent; }");
                        
                        sourceLabel->setText("Generated by: " + modelName + " (unmoderated)");
                        sourceLabel->setStyleSheet("color: #c62828; font-size: 9pt;");
                        sourceLabel->show();
                        
                        // Toggle buttons
                        showAnywayBtn->hide();
                        hideImageBtn->show();
                        
                        // Setup download button
                        QByteArray finalImageData = imageWithMetadata.isEmpty() ? imageData : imageWithMetadata;
                        downloadBtn->show();
                        connect(downloadBtn, &QPushButton::clicked, [finalImageData, prompt]() {
                            QString safeName = prompt.left(30).simplified();
                            safeName.replace(QRegularExpression("[^a-zA-Z0-9 ]"), "");
                            safeName.replace(" ", "_");
                            if (safeName.isEmpty()) safeName = "generated_image";
                            
                            QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) 
                                + "/" + safeName + ".png";
                            
                            QString filename = QFileDialog::getSaveFileName(
                                nullptr, "Save Image", defaultPath,
                                "PNG Image (*.png);;JPEG Image (*.jpg);;All Files (*)"
                            );
                            
                            if (!filename.isEmpty()) {
                                QFile saveFile(filename);
                                if (saveFile.open(QIODevice::WriteOnly)) {
                                    saveFile.write(finalImageData);
                                    saveFile.close();
                                    Logger::info("User saved blocked image to: " + filename.toStdString());
                                }
                            }
                        });
                        
                        statusLabel_->setText("Showing blocked image");
                        Logger::info("User chose to view blocked image");
                    }
                });
                
                // Connect hide image button
                connect(hideImageBtn, &QPushButton::clicked, [imageLabel, sourceLabel, downloadBtn, showAnywayBtn, hideImageBtn, bubble]() {
                    // Restore blocked state
                    imageLabel->clear();
                    imageLabel->setText("[Image blocked by Hive moderation]\n\nThe generated image was flagged for potentially inappropriate content.");
                    imageLabel->setStyleSheet(
                        "QLabel { "
                        "  background-color: #ffebee; "
                        "  border-radius: 12px; "
                        "  padding: 20px; "
                        "  color: #c62828; "
                        "  font-weight: bold; "
                        "}"
                    );
                    bubble->setStyleSheet(
                        "QFrame { "
                        "  background-color: #ffebee; "
                        "  color: #c62828; "
                        "  border-radius: 18px; "
                        "  border: 1px solid #ef9a9a; "
                        "}"
                    );
                    
                    // Toggle buttons
                    hideImageBtn->hide();
                    showAnywayBtn->show();
                    downloadBtn->hide();
                    sourceLabel->hide();
                    
                    Logger::info("User hid blocked image");
                });
                
                
                statusLabel_->setText("Image blocked by moderation");
                emit imageBlocked("Content policy violation");
            } else {
                // Embed metadata and display
                QByteArray imageWithMetadata = embedImageMetadata(imageData, selectedModel);
                
                // Load and display the image
                QImage image;
                image.loadFromData(imageWithMetadata.isEmpty() ? imageData : imageWithMetadata);
                
                if (image.isNull()) {
                    // Try original data
                    QBuffer buffer;
                    buffer.setData(imageData);
                    buffer.open(QIODevice::ReadOnly);
                    QImageReader reader(&buffer);
                    reader.setAutoDetectImageFormat(true);
                    if (imageUrl.toLower().endsWith(".webp")) {
                        reader.setFormat("webp");
                    }
                    image = reader.read();
                }
                
                if (!image.isNull()) {
                    QPixmap pixmap = QPixmap::fromImage(image);
                    QPixmap scaled = pixmap.scaled(380, 380, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    imageLabel->setPixmap(scaled);
                    imageLabel->setStyleSheet("QLabel { background: transparent; }");
                    
                    // Show source info
                    sourceLabel->setText("Generated by: " + modelName);
                    sourceLabel->show();
                    
                    // Save the watermarked image locally (auto-save)
                    QString savePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/generated_images";
                    QDir().mkpath(savePath);
                    QString autoSaveFilename = savePath + "/" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".png";
                    
                    QByteArray finalImageData = imageWithMetadata.isEmpty() ? imageData : imageWithMetadata;
                    
                    QFile file(autoSaveFilename);
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(finalImageData);
                        file.close();
                        Logger::info("Saved watermarked image to: " + autoSaveFilename.toStdString());
                    }
                    
                    // Show download button and connect it
                    downloadBtn->show();
                    connect(downloadBtn, &QPushButton::clicked, [finalImageData, prompt]() {
                        // Create a sanitized filename from prompt
                        QString safeName = prompt.left(30).simplified();
                        safeName.replace(QRegularExpression("[^a-zA-Z0-9 ]"), "");
                        safeName.replace(" ", "_");
                        if (safeName.isEmpty()) safeName = "generated_image";
                        
                        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) 
                            + "/" + safeName + ".png";
                        
                        QString filename = QFileDialog::getSaveFileName(
                            nullptr,
                            "Save Image",
                            defaultPath,
                            "PNG Image (*.png);;JPEG Image (*.jpg);;All Files (*)"
                        );
                        
                        if (!filename.isEmpty()) {
                            QFile saveFile(filename);
                            if (saveFile.open(QIODevice::WriteOnly)) {
                                saveFile.write(finalImageData);
                                saveFile.close();
                                Logger::info("User saved image to: " + filename.toStdString());
                            }
                        }
                    });
                    
                    statusLabel_->setText("Image generated");
                } else {
                    imageLabel->setText("<a href='" + imageUrl + "' style='color: #0066cc;'>Click to view image</a>");
                    imageLabel->setOpenExternalLinks(true);
                    imageLabel->setMinimumSize(200, 50);
                }
            }
        } else {
            imageLabel->setText("Could not load image\n<a href='" + imageUrl + "' style='color: #0066cc;'>Open in browser</a>");
            imageLabel->setOpenExternalLinks(true);
            imageLabel->setMinimumSize(200, 50);
            Logger::error("Image download error: " + reply->errorString().toStdString());
            statusLabel_->setText("Download failed");
        }
        
        reply->deleteLater();
        manager->deleteLater();
        scrollToBottom();
    });
    
    manager->get(request);
    scrollToBottom();
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
    
    auto* typingLabel = new QLabel("AI is thinking...", typingIndicator_);
    QFont font = typingLabel->font();
    font.setPointSize(13);
    font.setItalic(true);
    typingLabel->setFont(font);
    
    bubbleLayout->addWidget(typingLabel);
    
    typingIndicator_->setStyleSheet(
        "QFrame { "
        "  background-color: #e8e8e8; "
        "  color: #666; "
        "  border-radius: 18px; "
        "  border: none; "
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
            "  font-size: 13pt; "
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
            "  font-size: 13pt; "
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
